#include "DashboardServer.h"
#include "../analysis/ReportLoader.h"
#include "../reporting/HtmlReportGenerator.h"
#include "../benchmarks/RegisterBenchmarks.h"
#include "../SystemInfo.h"
#include "../custom/CustomAlgorithm.h"
#include "../custom/InterfaceDetector.h"
#include "../custom/CustomBenchmarkCompiler.h"
#include "../custom/CustomBenchmarkRunner.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <shellapi.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace server {

namespace {

std::string state_to_string(EngineState s) {
    switch (s) {
        case EngineState::Idle:      return "idle";
        case EngineState::Running:   return "running";
        case EngineState::Completed: return "completed";
        case EngineState::Cancelled: return "cancelled";
        case EngineState::Failed:    return "failed";
    }
    return "unknown";
}

std::string escape_json_str(const std::string& input) {
    std::ostringstream ss;
    for (char c : input) {
        switch (c) {
            case '"':  ss << "\\\""; break;
            case '\\': ss << "\\\\"; break;
            case '\b': ss << "\\b";  break;
            case '\f': ss << "\\f";  break;
            case '\n': ss << "\\n";  break;
            case '\r': ss << "\\r";  break;
            case '\t': ss << "\\t";  break;
            default:
                if ('\x00' <= c && c <= '\x1f') {
                    ss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
                } else {
                    ss << c;
                }
        }
    }
    return ss.str();
}

std::string unescape_json_str(const std::string& input) {
    std::string output;
    output.reserve(input.size());
    for (size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '\\' && i + 1 < input.size()) {
            char next = input[++i];
            switch (next) {
                case '"':  output += '"'; break;
                case '\\': output += '\\'; break;
                case '/':  output += '/'; break;
                case 'b':  output += '\b'; break;
                case 'f':  output += '\f'; break;
                case 'n':  output += '\n'; break;
                case 'r':  output += '\r'; break;
                case 't':  output += '\t'; break;
                default:   output += next; break;
            }
        } else {
            output += input[i];
        }
    }
    return output;
}

size_t find_closing_quote(const std::string& json, size_t start_quote) {
    bool escaped = false;
    for (size_t i = start_quote + 1; i < json.size(); ++i) {
        if (escaped) {
            escaped = false;
        } else if (json[i] == '\\') {
            escaped = true;
        } else if (json[i] == '"') {
            return i;
        }
    }
    return std::string::npos;
}

std::string extract_json_string(const std::string& json, const std::string& key, const std::string& def = "") {
    std::string pattern = "\"" + key + "\"";
    size_t k_pos = json.find(pattern);
    if (k_pos == std::string::npos) return def;
    size_t colon_pos = json.find(':', k_pos);
    if (colon_pos == std::string::npos) return def;
    size_t q1 = json.find('"', colon_pos);
    if (q1 == std::string::npos) return def;
    size_t q2 = find_closing_quote(json, q1);
    if (q2 == std::string::npos) return def;
    return unescape_json_str(json.substr(q1 + 1, q2 - q1 - 1));
}

std::string sanitize_filename(const std::string& raw) {
    std::string clean;
    for (char c : raw) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
            clean += '_';
        } else {
            clean += c;
        }
    }
    if (clean.empty() || clean == "." || clean == "..") {
        clean = "custom_dataset.txt";
    }
    return clean;
}


int extract_json_int(const std::string& json, const std::string& key, int def = 0) {
    std::string pattern = "\"" + key + "\"";
    size_t k_pos = json.find(pattern);
    if (k_pos == std::string::npos) return def;
    size_t colon_pos = json.find(':', k_pos);
    if (colon_pos == std::string::npos) return def;
    size_t start = colon_pos + 1;
    while (start < json.size() && (json[start] == ' ' || json[start] == '\t' || json[start] == '\r' || json[start] == '\n')) {
        ++start;
    }
    if (start >= json.size()) return def;
    try {
        size_t len = 0;
        int val = std::stoi(json.substr(start), &len);
        return val;
    } catch (...) {
        return def;
    }
}

bool extract_json_bool(const std::string& json, const std::string& key, bool def = false) {
    std::string pattern = "\"" + key + "\"";
    size_t k_pos = json.find(pattern);
    if (k_pos == std::string::npos) return def;
    size_t colon_pos = json.find(':', k_pos);
    if (colon_pos == std::string::npos) return def;
    size_t start = colon_pos + 1;
    while (start < json.size() && (json[start] == ' ' || json[start] == '\t' || json[start] == '\r' || json[start] == '\n')) {
        ++start;
    }
    if (json.compare(start, 4, "true") == 0) return true;
    if (json.compare(start, 5, "false") == 0) return false;
    return def;
}

std::vector<std::string> extract_json_string_array(const std::string& json, const std::string& key) {
    std::vector<std::string> items;
    std::string pattern = "\"" + key + "\"";
    size_t k_pos = json.find(pattern);
    if (k_pos == std::string::npos) return items;
    size_t b1 = json.find('[', k_pos);
    if (b1 == std::string::npos) return items;
    size_t b2 = json.find(']', b1);
    if (b2 == std::string::npos) return items;
    std::string sub = json.substr(b1 + 1, b2 - b1 - 1);

    size_t pos = 0;
    while (pos < sub.size()) {
        size_t q1 = sub.find('"', pos);
        if (q1 == std::string::npos) break;
        size_t q2 = sub.find('"', q1 + 1);
        if (q2 == std::string::npos) break;
        items.push_back(sub.substr(q1 + 1, q2 - q1 - 1));
        pos = q2 + 1;
    }
    return items;
}

std::vector<size_t> extract_json_size_array(const std::string& json, const std::string& key) {
    std::vector<size_t> items;
    std::string pattern = "\"" + key + "\"";
    size_t k_pos = json.find(pattern);
    if (k_pos == std::string::npos) return items;
    size_t b1 = json.find('[', k_pos);
    if (b1 == std::string::npos) return items;
    size_t b2 = json.find(']', b1);
    if (b2 == std::string::npos) return items;
    std::string sub = json.substr(b1 + 1, b2 - b1 - 1);
    std::stringstream ss(sub);
    std::string token;
    while (std::getline(ss, token, ',')) {
        try {
            size_t val = std::stoull(token);
            if (val > 0) items.push_back(val);
        } catch (...) {}
    }
    return items;
}

std::string chart_data_to_json(const analysis::ChartData& chart) {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"title\": \"" << escape_json_str(chart.title) << "\",\n";
    ss << "  \"x_label\": \"" << escape_json_str(chart.x_label) << "\",\n";
    ss << "  \"y_label\": \"" << escape_json_str(chart.y_label) << "\",\n";
    ss << "  \"valid\": " << (chart.valid ? "true" : "false") << ",\n";
    ss << "  \"error_message\": \"" << escape_json_str(chart.error_message) << "\",\n";
    ss << "  \"series\": [\n";

    for (size_t i = 0; i < chart.series.size(); ++i) {
        const auto& s = chart.series[i];
        ss << "    {\n";
        ss << "      \"algorithm\": \"" << escape_json_str(s.algorithm_key) << "\",\n";
        ss << "      \"name\": \"" << escape_json_str(s.algorithm_name) << "\",\n";
        ss << "      \"points\": [\n";
        for (size_t j = 0; j < s.points.size(); ++j) {
            ss << "        {\"x\": " << std::fixed << std::setprecision(0) << s.points[j].x
               << ", \"y\": " << std::fixed << std::setprecision(4) << s.points[j].y << "}"
               << (j + 1 < s.points.size() ? "," : "") << "\n";
        }
        ss << "      ]\n";
        ss << "    }" << (i + 1 < chart.series.size() ? "," : "") << "\n";
    }
    ss << "  ]\n";
    ss << "}";
    return ss.str();
}

std::string comparison_to_json(const analysis::ComparisonReport& rep) {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"valid\": " << (rep.valid ? "true" : "false") << ",\n";
    ss << "  \"run_id\": \"" << escape_json_str(rep.run_id) << "\",\n";
    ss << "  \"groups\": [\n";
    for (size_t i = 0; i < rep.groups.size(); ++i) {
        const auto& g = rep.groups[i];
        ss << "    {\n";
        ss << "      \"input_type\": \"" << escape_json_str(g.input_type) << "\",\n";
        ss << "      \"input_size\": " << g.input_size << ",\n";
        ss << "      \"fastest\": \"" << escape_json_str(g.fastest_algorithm) << "\",\n";
        ss << "      \"slowest\": \"" << escape_json_str(g.slowest_algorithm) << "\",\n";
        ss << "      \"max_speedup\": " << std::fixed << std::setprecision(2) << g.max_speedup << ",\n";
        ss << "      \"rankings\": [\n";
        for (size_t j = 0; j < g.rankings.size(); ++j) {
            const auto& rk = g.rankings[j];
            ss << "        {\n";
            ss << "          \"rank\": " << rk.rank << ",\n";
            ss << "          \"algorithm\": \"" << escape_json_str(rk.algorithm_key) << "\",\n";
            ss << "          \"name\": \"" << escape_json_str(rk.algorithm_name) << "\",\n";
            ss << "          \"algorithm_name\": \"" << escape_json_str(rk.algorithm_name) << "\",\n";
            ss << "          \"time_mean_ns\": " << std::fixed << std::setprecision(0) << rk.time_mean_ns << ",\n";
            ss << "          \"mean_time_ns\": " << std::fixed << std::setprecision(0) << rk.time_mean_ns << ",\n";
            ss << "          \"time_median_ns\": " << std::fixed << std::setprecision(0) << rk.time_median_ns << ",\n";
            ss << "          \"median_time_ns\": " << std::fixed << std::setprecision(0) << rk.time_median_ns << ",\n";
            ss << "          \"speedup\": " << std::fixed << std::setprecision(2) << rk.speedup_vs_slowest << ",\n";
            ss << "          \"speedup_factor\": " << std::fixed << std::setprecision(2) << rk.speedup_vs_slowest << ",\n";
            ss << "          \"percentage_faster\": " << std::fixed << std::setprecision(1) << rk.percentage_faster_than_slowest << ",\n";
            ss << "          \"percentage_faster_than_slowest\": " << std::fixed << std::setprecision(1) << rk.percentage_faster_than_slowest << "\n";
            ss << "        }" << (j + 1 < g.rankings.size() ? "," : "") << "\n";
        }
        ss << "      ]\n";
        ss << "    }" << (i + 1 < rep.groups.size() ? "," : "") << "\n";
    }
    ss << "  ]\n";
    ss << "}";
    return ss.str();
}

std::string regression_to_json(const analysis::RegressionReport& rep) {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"valid\": " << (rep.valid ? "true" : "false") << ",\n";
    ss << "  \"baseline_run_id\": \"" << escape_json_str(rep.baseline_run_id) << "\",\n";
    ss << "  \"current_run_id\": \"" << escape_json_str(rep.current_run_id) << "\",\n";
    ss << "  \"tolerance\": " << std::fixed << std::setprecision(1) << rep.tolerance_percentage << ",\n";
    ss << "  \"has_regressions\": " << (rep.has_regressions ? "true" : "false") << ",\n";
    ss << "  \"improved_count\": " << rep.improved_count << ",\n";
    ss << "  \"stable_count\": " << rep.stable_count << ",\n";
    ss << "  \"regressed_count\": " << rep.regressed_count << ",\n";
    ss << "  \"regression_count\": " << rep.regressed_count << ",\n";
    ss << "  \"records\": [\n";
    for (size_t i = 0; i < rep.records.size(); ++i) {
        const auto& r = rep.records[i];
        std::string st = "stable";
        if (r.status == analysis::RegressionStatus::Improved) st = "improved";
        else if (r.status == analysis::RegressionStatus::Regressed) st = "regressed";

        ss << "    {\n";
        ss << "      \"algorithm\": \"" << escape_json_str(r.algorithm_key) << "\",\n";
        ss << "      \"name\": \"" << escape_json_str(r.algorithm_name) << "\",\n";
        ss << "      \"baseline_time_ns\": " << std::fixed << std::setprecision(0) << r.baseline_time_ns << ",\n";
        ss << "      \"current_time_ns\": " << std::fixed << std::setprecision(0) << r.current_time_ns << ",\n";
        ss << "      \"delta_percentage\": " << std::fixed << std::setprecision(2) << r.delta_percentage << ",\n";
        ss << "      \"status\": \"" << st << "\"\n";
        ss << "    }" << (i + 1 < rep.records.size() ? "," : "") << "\n";
    }
    ss << "  ]\n";
    ss << "}";
    return ss.str();
}

} // namespace (anonymous)

DashboardServer::DashboardServer(int port, std::string web_root_dir)
    : requested_port(port), web_root(std::move(web_root_dir)) {
    analysis::HistoryManager history;
    auto latest = history.get_latest_run();
    if (latest.has_value()) {
        current_progress.last_run_id = latest->run_id;
    }
}

DashboardServer::~DashboardServer() {
    stop();
}

std::string DashboardServer::url() const {
    return "http://localhost:" + std::to_string(active_port);
}

BenchmarkProgress DashboardServer::get_progress() const {
    std::lock_guard<std::mutex> lock(status_mutex);
    return current_progress;
}

bool DashboardServer::start() {
    if (running.load()) return true;

#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "Winsock initialization failed.\n";
        return false;
    }
#endif

    int try_port = requested_port;
    SOCKET listen_sock = INVALID_SOCKET;

    for (int attempt = 0; attempt < 2; ++attempt) {
        listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listen_sock == INVALID_SOCKET) {
            break;
        }

#ifndef _WIN32
        int opt = 1;
        setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));
#endif

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        addr.sin_port = htons(static_cast<u_short>(try_port));

        if (bind(listen_sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0) {
            if (listen(listen_sock, SOMAXCONN) == 0) {
                active_port = try_port;
                break;
            }
        }

#ifdef _WIN32
        closesocket(listen_sock);
#else
        close(listen_sock);
#endif
        listen_sock = INVALID_SOCKET;
        try_port++; // Fallback port (e.g. 8081)
    }

    if (listen_sock == INVALID_SOCKET) {
        std::cerr << "Could not bind server to port " << requested_port << " or " << (requested_port + 1) << ".\n";
#ifdef _WIN32
        WSACleanup();
#endif
        return false;
    }

    server_socket = static_cast<uintptr_t>(listen_sock);
    running.store(true);

    listener_thread = std::make_unique<std::thread>(&DashboardServer::run_listener, this);
    return true;
}

void DashboardServer::stop() {
    if (!running.exchange(false)) return;

    if (server_socket != 0 && server_socket != static_cast<uintptr_t>(INVALID_SOCKET)) {
#ifdef _WIN32
        closesocket(static_cast<SOCKET>(server_socket));
#else
        close(static_cast<int>(server_socket));
#endif
        server_socket = 0;
    }

    if (listener_thread && listener_thread->joinable()) {
        listener_thread->join();
    }
    listener_thread.reset();

    // Wait up to 1 second for active client threads to finish
    auto start_wait = std::chrono::steady_clock::now();
    while (active_client_count.load() > 0 &&
           std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_wait).count() < 1000) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    if (worker_thread && worker_thread->joinable()) {
        worker_thread->join();
    }
    worker_thread.reset();

#ifdef _WIN32
    WSACleanup();
#endif
}

void DashboardServer::wait() {
    while (running.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    stop();
}

void DashboardServer::run_listener() {
    SOCKET listen_sock = static_cast<SOCKET>(server_socket);

    while (running.load()) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(listen_sock, &read_fds);

        timeval timeout{};
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000; // 100 ms timeout to check running flag

        int sel = select(0, &read_fds, nullptr, nullptr, &timeout);
        if (sel > 0 && FD_ISSET(listen_sock, &read_fds)) {
            sockaddr_in client_addr{};
            int client_len = sizeof(client_addr);
            SOCKET client_sock = accept(listen_sock, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
            if (client_sock != INVALID_SOCKET) {
#ifdef _WIN32
                DWORD tv = 5000;
                setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));
                setsockopt(client_sock, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));
#else
                timeval tv{};
                tv.tv_sec = 5;
                setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));
                setsockopt(client_sock, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));
#endif
                active_client_count++;
                std::thread([this, client_sock]() {
                    handle_client(static_cast<uintptr_t>(client_sock));
                    active_client_count--;
                }).detach();
            }
        }
    }
}

void DashboardServer::handle_client(uintptr_t sock_ptr) {
    SOCKET client_sock = static_cast<SOCKET>(sock_ptr);

    std::string request_str;
    char buffer[4096];
    size_t header_end = std::string::npos;

    while (header_end == std::string::npos) {
        int bytes_read = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
        if (bytes_read <= 0) {
#ifdef _WIN32
            closesocket(client_sock);
#else
            close(client_sock);
#endif
            return;
        }
        request_str.append(buffer, static_cast<size_t>(bytes_read));
        header_end = request_str.find("\r\n\r\n");
        if (request_str.size() > 65536) {
            break;
        }
    }

    if (header_end == std::string::npos) {
#ifdef _WIN32
        closesocket(client_sock);
#else
        close(client_sock);
#endif
        return;
    }

    std::string headers = request_str.substr(0, header_end);
    std::string body = request_str.substr(header_end + 4);

    std::istringstream req_stream(headers);
    std::string method, full_uri, protocol;
    req_stream >> method >> full_uri >> protocol;

    std::string path = full_uri;
    std::string query_str;
    size_t q_pos = full_uri.find('?');
    if (q_pos != std::string::npos) {
        path = full_uri.substr(0, q_pos);
        query_str = full_uri.substr(q_pos + 1);
    }
    auto query_map = parse_query(query_str);

    size_t cl_pos = headers.find("Content-Length:");
    if (cl_pos == std::string::npos) cl_pos = headers.find("content-length:");
    if (cl_pos != std::string::npos) {
        size_t end_line = headers.find("\r\n", cl_pos);
        std::string cl_str = (end_line != std::string::npos)
            ? headers.substr(cl_pos + 15, end_line - (cl_pos + 15))
            : headers.substr(cl_pos + 15);
        try {
            size_t content_length = std::stoull(cl_str);
            while (body.size() < content_length) {
                int more = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
                if (more <= 0) break;
                body.append(buffer, static_cast<size_t>(more));
            }
        } catch (...) {}
    }

    if (method == "GET" && path == "/api/status") {
        send_response(sock_ptr, 200, "application/json", handle_status());
    } else if (method == "GET" && path == "/api/history") {
        send_response(sock_ptr, 200, "application/json", handle_history());
    } else if (method == "GET" && path == "/api/run") {
        send_response(sock_ptr, 200, "application/json", handle_run(query_map["id"]));
    } else if (method == "GET" && path == "/api/compare") {
        send_response(sock_ptr, 200, "application/json", handle_compare(query_map["id"]));
    } else if (method == "GET" && path == "/api/regression") {
        send_response(sock_ptr, 200, "application/json", handle_regression(query_map["id"]));
    } else if (method == "GET" && path == "/api/trend") {
        std::string metric = query_map.count("metric") ? query_map["metric"] : "time";
        std::string dist = query_map.count("dist") ? query_map["dist"] : "";
        send_response(sock_ptr, 200, "application/json", handle_trend(metric, dist));
    } else if (method == "POST" && path == "/api/validate-file") {
        send_response(sock_ptr, 200, "application/json", handle_validate_file(body));
    } else if (method == "POST" && path == "/api/upload-dataset") {
        send_response(sock_ptr, 200, "application/json", handle_upload_dataset(body));
    } else if (method == "POST" && path == "/api/benchmark") {
        send_response(sock_ptr, 200, "application/json", handle_benchmark(body));
    } else if (method == "POST" && path == "/api/custom-benchmark/start") {
        send_response(sock_ptr, 200, "application/json", handle_custom_benchmark(body));
    } else if (method == "POST" && path == "/api/custom-benchmark/detect-interface") {
        send_response(sock_ptr, 200, "application/json", handle_detect_interface(body));
    } else if (method == "POST" && path == "/api/custom-benchmark/detect-target") {
        send_response(sock_ptr, 200, "application/json", handle_detect_target(body));
    } else if (method == "POST" && path == "/api/custom-benchmark/upload-algorithm") {
        send_response(sock_ptr, 200, "application/json", handle_upload_custom_algorithm(body));
    } else if (method == "GET" && path == "/api/custom-benchmark/samples") {
        send_response(sock_ptr, 200, "application/json", handle_custom_samples());
    } else if (method == "POST" && path == "/api/benchmark/cancel") {
        send_response(sock_ptr, 200, "application/json", handle_cancel());
    } else if (method == "POST" && path == "/api/report") {
        send_response(sock_ptr, 200, "application/json", handle_report(query_map["id"]));
    } else if (method == "OPTIONS") {
        send_response(sock_ptr, 204, "text/plain", "");
    } else {
        if (!serve_static_file(sock_ptr, path)) {
            send_response(sock_ptr, 404, "text/plain", "404 Not Found");
        }
    }

#ifdef _WIN32
    closesocket(client_sock);
#else
    close(client_sock);
#endif
}

std::string DashboardServer::handle_status() {
    std::lock_guard<std::mutex> lock(status_mutex);
    std::ostringstream ss;
    ss << "{\n"
       << "  \"state\": \"" << state_to_string(current_progress.state) << "\",\n"
       << "  \"progress\": " << current_progress.progress_percent << ",\n"
       << "  \"current_algorithm\": \"" << escape_json_str(current_progress.current_algorithm) << "\",\n"
       << "  \"current_input_type\": \"" << escape_json_str(current_progress.current_input_type) << "\",\n"
       << "  \"current_size\": " << current_progress.current_size << ",\n"
       << "  \"current_iteration\": " << current_progress.current_iteration << ",\n"
       << "  \"total_iterations\": " << current_progress.total_iterations << ",\n"
       << "  \"elapsed_seconds\": " << std::fixed << std::setprecision(2) << current_progress.elapsed_seconds << ",\n"
       << "  \"last_run_id\": \"" << escape_json_str(current_progress.last_run_id) << "\",\n"
       << "  \"error_message\": \"" << escape_json_str(current_progress.error_message) << "\",\n";
    SystemInfo sys;
    ss << "  \"system\": {\n"
       << "    \"os\": \"" << escape_json_str(sys.operating_system()) << "\",\n"
       << "    \"cpu\": \"" << escape_json_str(sys.cpu_name()) << "\",\n"
       << "    \"architecture\": \"" << escape_json_str(sys.architecture()) << "\",\n"
       << "    \"physical_cores\": " << sys.physical_cores() << ",\n"
       << "    \"logical_cpus\": " << sys.logical_processors() << ",\n"
       << "    \"compiler\": \"" << escape_json_str(sys.compiler()) << "\",\n"
       << "    \"cxx_standard\": \"" << escape_json_str(sys.cxx_standard()) << "\",\n"
       << "    \"optimization\": \"" << escape_json_str(sys.optimization()) << "\"\n"
       << "  }\n"
       << "}";
    return ss.str();
}

std::string DashboardServer::handle_history() {
    analysis::HistoryManager history;
    const auto metas = history.list_runs();

    std::ostringstream ss;
    ss << "{\n  \"runs\": [\n";
    for (size_t i = 0; i < metas.size(); ++i) {
        const auto& m = metas[i];
        ss << "    {\n"
           << "      \"run_id\": \"" << escape_json_str(m.run_id) << "\",\n"
           << "      \"timestamp\": \"" << escape_json_str(m.timestamp) << "\",\n"
           << "      \"input_type\": \"" << escape_json_str(m.input_type) << "\",\n"
           << "      \"file_path\": \"" << escape_json_str(m.file_path) << "\",\n"
           << "      \"element_count\": " << m.element_count << ",\n"
           << "      \"algorithm_count\": " << m.algorithm_count << ",\n"
           << "      \"algorithms\": [";
        for (size_t j = 0; j < m.algorithms.size(); ++j) {
            ss << "\"" << escape_json_str(m.algorithms[j]) << "\"" << (j + 1 < m.algorithms.size() ? ", " : "");
        }
        ss << "]\n"
           << "    }" << (i + 1 < metas.size() ? "," : "") << "\n";
    }
    ss << "  ]\n}";
    return ss.str();
}

std::string DashboardServer::handle_run(const std::string& query_id) {
    analysis::HistoryManager history;
    analysis::LoadResult res;
    if (query_id.empty()) {
        auto latest = history.get_latest_run();
        if (!latest.has_value()) return "{\"error\": \"No runs in history\"}";
        return analysis::ReportLoader::to_json_string(*latest);
    } else {
        res = history.load_run(query_id);
        if (!res.success) {
            return "{\"error\": \"" + escape_json_str(res.error_message) + "\"}";
        }
        return analysis::ReportLoader::to_json_string(res.run);
    }
}

std::string DashboardServer::handle_compare(const std::string& query_id) {
    analysis::HistoryManager history;
    analysis::LoadResult res;
    if (query_id.empty()) {
        auto latest = history.get_latest_run();
        if (!latest.has_value()) return "{\"error\": \"No runs in history\"}";
        res.run = *latest;
    } else {
        res = history.load_run(query_id);
        if (!res.success) return "{\"error\": \"" + escape_json_str(res.error_message) + "\"}";
    }

    auto comp = analysis::ComparisonAnalyzer::compare_run(res.run);
    return comparison_to_json(comp);
}

std::string DashboardServer::handle_regression(const std::string& query_id) {
    analysis::HistoryManager history;
    analysis::LoadResult curr_res;
    if (query_id.empty()) {
        auto latest = history.get_latest_run();
        if (!latest.has_value()) return "{\"error\": \"No runs in history\"}";
        curr_res.run = *latest;
    } else {
        curr_res = history.load_run(query_id);
        if (!curr_res.success) return "{\"error\": \"" + escape_json_str(curr_res.error_message) + "\"}";
    }

    auto metas = history.list_runs();
    analysis::BenchmarkRun baseline_run;
    bool found_base = false;

    for (const auto& m : metas) {
        if (m.run_id != curr_res.run.run_id) {
            auto l = history.load_run(m.run_id);
            if (l.success && analysis::RegressionAnalyzer::are_runs_compatible(curr_res.run, l.run)) {
                baseline_run = l.run;
                found_base = true;
                break;
            }
        }
    }

    if (!found_base) {
        return "{\"valid\": false, \"error\": \"No compatible baseline run found in history.\"}";
    }

    auto rep = analysis::RegressionAnalyzer::compare_runs(curr_res.run, baseline_run);
    return regression_to_json(rep);
}

std::string DashboardServer::handle_trend(const std::string& metric, const std::string& distribution) {
    analysis::HistoryManager history;
    auto metas = history.list_runs();
    std::vector<analysis::BenchmarkRun> all_runs;
    for (const auto& m : metas) {
        auto r = history.load_run(m.run_id);
        if (r.success) all_runs.push_back(r.run);
    }

    analysis::ChartFilter filter;
    if (!distribution.empty()) {
        filter.input_distribution = distribution;
    }

    if (metric == "memory") {
        auto chart = analysis::TrendAnalyzer::build_memory_vs_size(all_runs, filter);
        return chart_data_to_json(chart);
    } else {
        auto chart = analysis::TrendAnalyzer::build_time_vs_size(all_runs, filter);
        return chart_data_to_json(chart);
    }
}

std::string DashboardServer::handle_validate_file(const std::string& body) {
    std::string file_path = extract_json_string(body, "file_path", "");
    if (file_path.empty()) {
        return "{\"valid\": false, \"error_message\": \"Missing or empty file_path in request\"}";
    }

    const ValidationResult val = FileInputLoader::load_and_validate(file_path);
    std::ostringstream ss;
    ss << "{\n"
       << "  \"valid\": " << (val.valid ? "true" : "false") << ",\n"
       << "  \"file_path\": \"" << escape_json_str(file_path) << "\",\n"
       << "  \"error_message\": \"" << escape_json_str(val.error_message) << "\",\n"
       << "  \"element_count\": " << val.info.element_count << ",\n"
       << "  \"distinct_count\": " << val.info.distinct_count << ",\n"
       << "  \"duplicate_count\": " << val.info.duplicate_count << ",\n"
       << "  \"min_value\": " << val.info.min_value << ",\n"
       << "  \"max_value\": " << val.info.max_value << ",\n"
       << "  \"order\": \"" << (val.info.is_all_equal ? "All Equal" :
                                val.info.is_sorted ? "Sorted" :
                                val.info.is_reverse_sorted ? "Reverse Sorted" : "Unsorted") << "\"\n"
       << "}";
    return ss.str();
}

std::string DashboardServer::handle_upload_dataset(const std::string& body) {
    std::string filename_raw = extract_json_string(body, "filename", "uploaded_dataset.txt");
    std::string content = extract_json_string(body, "content", "");

    if (content.empty()) {
        return "{\"valid\": false, \"error_message\": \"Uploaded file content is empty\"}";
    }

    std::string sanitized = sanitize_filename(filename_raw);
    std::string dir = "datasets";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);

    std::string file_path = dir + "/" + sanitized;
    std::ofstream out(file_path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        return "{\"valid\": false, \"error_message\": \"Failed to save dataset file to server\"}";
    }
    out << content;
    out.close();

    const ValidationResult val = FileInputLoader::load_and_validate(file_path);
    std::ostringstream ss;
    ss << "{\n"
       << "  \"valid\": " << (val.valid ? "true" : "false") << ",\n"
       << "  \"file_path\": \"" << escape_json_str(file_path) << "\",\n"
       << "  \"filename\": \"" << escape_json_str(sanitized) << "\",\n"
       << "  \"error_message\": \"" << escape_json_str(val.error_message) << "\",\n"
       << "  \"element_count\": " << val.info.element_count << ",\n"
       << "  \"distinct_count\": " << val.info.distinct_count << ",\n"
       << "  \"duplicate_count\": " << val.info.duplicate_count << ",\n"
       << "  \"min_value\": " << val.info.min_value << ",\n"
       << "  \"max_value\": " << val.info.max_value << ",\n"
       << "  \"order\": \"" << (val.info.is_all_equal ? "All Equal" :
                                val.info.is_sorted ? "Sorted" :
                                val.info.is_reverse_sorted ? "Reverse Sorted" : "Unsorted") << "\"\n"
       << "}";
    return ss.str();
}

std::string DashboardServer::handle_benchmark(const std::string& body) {
    {
        std::lock_guard<std::mutex> lock(status_mutex);
        if (current_progress.state == EngineState::Running) {
            return "{\"status\": \"busy\", \"message\": \"A benchmark is already running\"}";
        }
    }

    std::string input_type = extract_json_string(body, "input_type", "Random");
    std::string file_path = extract_json_string(body, "file_path", "");
    std::vector<std::string> algorithms = extract_json_string_array(body, "algorithms");
    std::vector<size_t> sizes = extract_json_size_array(body, "sizes");
    int iterations = extract_json_int(body, "iterations", 20);
    int warmup = extract_json_int(body, "warmup", 5);
    bool memory = extract_json_bool(body, "memory", true);
    int cpu_affinity = extract_json_int(body, "cpu_affinity", -1);

    BenchmarkConfig config;
    config.iterations = (iterations > 0 && iterations <= 1000) ? iterations : 20;
    config.warmup_runs = (warmup >= 0 && warmup <= 100) ? warmup : 5;
    config.measure_memory = memory;
    config.cpu_affinity = cpu_affinity;

    if (input_type == "CustomFile") {
        if (file_path.empty()) {
            return "{\"status\": \"error\", \"message\": \"File path cannot be empty for CustomFile\"}";
        }
        const ValidationResult val = FileInputLoader::load_and_validate(file_path);
        if (!val.valid) {
            return "{\"status\": \"error\", \"message\": \"" + escape_json_str(val.error_message) + "\"}";
        }
        config.is_custom_file = true;
        config.custom_file_path = file_path;
        config.input_cases = { InputDataCase::CustomFile };
        config.input_sizes = { val.info.element_count };
        FileInputLoader::set_active_dataset(std::move(val.data), val.info, file_path);
    } else {
        InputDataCase idc = InputDataCase::Random;
        if (input_type == "Sorted") idc = InputDataCase::Sorted;
        else if (input_type == "ReverseSorted" || input_type == "Reverse Sorted") idc = InputDataCase::ReverseSorted;
        else if (input_type == "NearlySorted" || input_type == "Nearly Sorted") idc = InputDataCase::NearlySorted;
        else if (input_type == "ManyDuplicates" || input_type == "Many Duplicates") idc = InputDataCase::ManyDuplicates;
        else if (input_type == "AllEqual" || input_type == "All Equal") idc = InputDataCase::AllEqual;
        config.input_cases = { idc };

        if (!sizes.empty()) {
            config.input_sizes = sizes;
        } else {
            config.input_sizes = { 100, 500, 1000 };
        }
    }

    cancel_requested.store(false);

    {
        std::lock_guard<std::mutex> lock(status_mutex);
        current_progress.state = EngineState::Running;
        current_progress.progress_percent = 0;
        current_progress.current_algorithm = "Initializing...";
        current_progress.current_input_type = input_type;
        current_progress.current_size = config.input_sizes.empty() ? 0 : config.input_sizes[0];
        current_progress.current_iteration = 0;
        current_progress.total_iterations = config.iterations;
        current_progress.elapsed_seconds = 0.0;
        current_progress.error_message = "";
    }

    if (worker_thread && worker_thread->joinable()) {
        worker_thread->join();
    }

    worker_thread = std::make_unique<std::thread>([this, config, algorithms]() {
        try {
            BenchmarkRunner runner(config);
            register_all_benchmarks(runner);

            runner.set_cancellation_check([this]() {
                return cancel_requested.load();
            });

            runner.set_progress_callback([this](
                int pct,
                const std::string& alg_name,
                const std::string& case_name,
                size_t size,
                int iter,
                int total_iters,
                double elapsed
            ) {
                std::lock_guard<std::mutex> lock(status_mutex);
                current_progress.progress_percent = pct;
                current_progress.current_algorithm = alg_name;
                current_progress.current_input_type = case_name;
                current_progress.current_size = size;
                current_progress.current_iteration = iter;
                current_progress.total_iterations = total_iters;
                current_progress.elapsed_seconds = elapsed;
            });

            if (algorithms.empty()) {
                runner.run_all();
            } else {
                runner.run_selected_keys(algorithms);
            }

            std::lock_guard<std::mutex> lock(status_mutex);
            current_progress.state = EngineState::Completed;
            current_progress.progress_percent = 100;
            current_progress.last_run_id = runner.last_analysis_run().run_id;
        } catch (const std::exception& ex) {
            std::lock_guard<std::mutex> lock(status_mutex);
            if (cancel_requested.load()) {
                current_progress.state = EngineState::Cancelled;
                current_progress.error_message = "Benchmark cancelled by user.";
            } else {
                current_progress.state = EngineState::Failed;
                current_progress.error_message = ex.what();
            }
        }
    });

    return "{\"status\": \"started\", \"message\": \"Benchmark started in background\"}";
}

std::string DashboardServer::handle_detect_interface(const std::string& body) {
    std::string source_code = extract_json_string(body, "source_code", "");
    std::string file_path = extract_json_string(body, "file_path", "");
    std::string cat_str = extract_json_string(body, "category", "search");
    custom::CustomCategory cat = custom::string_to_custom_category(cat_str);

    custom::DetectionResult res;
    if (!source_code.empty()) {
        res = custom::InterfaceDetector::detect(source_code, cat);
    } else if (!file_path.empty()) {
        res = custom::InterfaceDetector::detect_from_file(file_path, cat);
    } else {
        return "{\"recognized\": false, \"diagnostic_message\": \"No source code or file path provided.\"}";
    }

    std::ostringstream ss;
    ss << "{\n"
       << "  \"recognized\": " << (res.recognized ? "true" : "false") << ",\n"
       << "  \"function_name\": \"" << escape_json_str(res.detected_function_name) << "\",\n"
       << "  \"signature\": \"" << escape_json_str(res.detected_signature) << "\",\n"
       << "  \"interface_type\": \"" << escape_json_str(custom::search_interface_type_to_string(res.interface_type)) << "\",\n"
       << "  \"interface_type_enum\": " << static_cast<int>(res.interface_type) << ",\n"
       << "  \"display_name\": \"" << escape_json_str(res.suggested_display_name) << "\",\n"
       << "  \"diagnostic_message\": \"" << escape_json_str(res.diagnostic_message) << "\",\n"
       << "  \"generated_adapter\": \"" << escape_json_str(res.generated_adapter_code) << "\"\n"
       << "}";
    return ss.str();
}

std::string DashboardServer::handle_upload_custom_algorithm(const std::string& body) {
    std::string filename_raw = extract_json_string(body, "filename", "custom_alg.cpp");
    std::string content = extract_json_string(body, "content", "");

    if (content.empty()) {
        return "{\"recognized\": false, \"diagnostic_message\": \"Uploaded file content is empty\"}";
    }

    std::string sanitized = sanitize_filename(filename_raw);
    std::string dir = "custom/uploads";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);

    std::string file_path = dir + "/" + sanitized;
    std::ofstream out(file_path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        return "{\"recognized\": false, \"diagnostic_message\": \"Failed to save algorithm file on server\"}";
    }
    out << content;
    out.close();

    auto det = custom::InterfaceDetector::detect(content, custom::CustomCategory::Search);

    std::ostringstream ss;
    ss << "{\n"
       << "  \"success\": " << (det.recognized ? "true" : "false") << ",\n"
       << "  \"recognized\": " << (det.recognized ? "true" : "false") << ",\n"
       << "  \"file_path\": \"" << escape_json_str(file_path) << "\",\n"
       << "  \"filename\": \"" << escape_json_str(sanitized) << "\",\n"
       << "  \"function_name\": \"" << escape_json_str(det.detected_function_name) << "\",\n"
       << "  \"signature\": \"" << escape_json_str(det.detected_signature) << "\",\n"
       << "  \"interface_type\": \"" << escape_json_str(custom::search_interface_type_to_string(det.interface_type)) << "\",\n"
       << "  \"interface_type_enum\": " << static_cast<int>(det.interface_type) << ",\n"
       << "  \"display_name\": \"" << escape_json_str(det.suggested_display_name) << "\",\n"
       << "  \"diagnostic_message\": \"" << escape_json_str(det.diagnostic_message) << "\",\n"
       << "  \"generated_adapter\": \"" << escape_json_str(det.generated_adapter_code) << "\"\n"
       << "}";
    return ss.str();
}

std::string DashboardServer::handle_detect_target(const std::string& body) {
    std::string dataset_path = extract_json_string(body, "dataset_path", "custom/samples/search_data.txt");
    int target = extract_json_int(body, "target", extract_json_int(body, "target_value", 5000));

    std::ifstream file(dataset_path);
    if (!file.is_open()) {
        return "{\"valid\": false, \"error_message\": \"Failed to open dataset file: " + escape_json_str(dataset_path) + "\"}";
    }

    std::vector<int> data;
    int val = 0;
    while (file >> val) {
        data.push_back(val);
    }
    file.close();

    if (data.empty()) {
        return "{\"valid\": false, \"error_message\": \"Dataset file is empty: " + escape_json_str(dataset_path) + "\"}";
    }

    bool available = false;
    size_t occurrences = 0;
    int first_index = -1;

    for (size_t i = 0; i < data.size(); ++i) {
        if (data[i] == target) {
            if (!available) {
                available = true;
                first_index = static_cast<int>(i);
            }
            occurrences++;
        }
    }

    std::string msg = available
        ? ("Target " + std::to_string(target) + " is present at index " + std::to_string(first_index) + " (" + std::to_string(occurrences) + " occurrence" + (occurrences > 1 ? "s" : "") + ")")
        : ("Target " + std::to_string(target) + " is absent from dataset (Negative search test)");

    std::ostringstream ss;
    ss << "{\n"
       << "  \"valid\": true,\n"
       << "  \"dataset_path\": \"" << escape_json_str(dataset_path) << "\",\n"
       << "  \"total_elements\": " << data.size() << ",\n"
       << "  \"target\": " << target << ",\n"
       << "  \"available\": " << (available ? "true" : "false") << ",\n"
       << "  \"occurrences\": " << occurrences << ",\n"
       << "  \"first_index\": " << first_index << ",\n"
       << "  \"message\": \"" << escape_json_str(msg) << "\"\n"
       << "}";
    return ss.str();
}

std::string DashboardServer::handle_custom_samples() {
    auto read_file_or_default = [](const std::string& path) -> std::string {
        std::ifstream f(path);
        if (!f.is_open()) return "";
        std::ostringstream ss;
        ss << f.rdbuf();
        return ss.str();
    };

    std::string lin_src = read_file_or_default("custom/samples/linear_search.cpp");
    std::string bin_src = read_file_or_default("custom/samples/binary_search.cpp");

    std::ostringstream ss;
    ss << "{\n"
       << "  \"algorithm_a\": {\n"
       << "    \"name\": \"Linear Search\",\n"
       << "    \"file_path\": \"custom/samples/linear_search.cpp\",\n"
       << "    \"function_name\": \"linear_search\",\n"
       << "    \"interface_type\": 3,\n"
       << "    \"source\": \"" << escape_json_str(lin_src) << "\"\n"
       << "  },\n"
       << "  \"algorithm_b\": {\n"
       << "    \"name\": \"Binary Search\",\n"
       << "    \"file_path\": \"custom/samples/binary_search.cpp\",\n"
       << "    \"function_name\": \"binarySearch\",\n"
       << "    \"interface_type\": 0,\n"
       << "    \"source\": \"" << escape_json_str(bin_src) << "\"\n"
       << "  },\n"
       << "  \"dataset\": {\n"
       << "    \"file_path\": \"custom/samples/search_data.txt\",\n"
       << "    \"target\": 5000\n"
       << "  }\n"
       << "}";
    return ss.str();
}

std::string DashboardServer::handle_custom_benchmark(const std::string& body) {
    {
        std::lock_guard<std::mutex> lock(status_mutex);
        if (current_progress.state == EngineState::Running) {
            return "{\"status\": \"busy\", \"message\": \"A benchmark is already running\"}";
        }
    }

    custom::CustomBenchmarkConfig cfg;
    cfg.dataset_path = extract_json_string(body, "dataset_path", "custom/samples/search_data.txt");
    cfg.target_value = extract_json_int(body, "target_value", extract_json_int(body, "target", 5000));
    cfg.iterations = extract_json_int(body, "iterations", 20);
    cfg.warmup_runs = extract_json_int(body, "warmup", 5);
    cfg.cpu_affinity = extract_json_int(body, "cpu_affinity", -1);
    cfg.measure_memory = extract_json_bool(body, "memory", true);
    cfg.timeout_seconds = extract_json_int(body, "timeout_seconds", 15);

    std::string alg_a_name = extract_json_string(body, "alg_a_name", "");
    std::string alg_a_path = extract_json_string(body, "alg_a_path", "");
    std::string alg_a_func = extract_json_string(body, "alg_a_func", "");
    int alg_a_type = extract_json_int(body, "alg_a_type", -1);

    std::string alg_b_name = extract_json_string(body, "alg_b_name", "");
    std::string alg_b_path = extract_json_string(body, "alg_b_path", "");
    std::string alg_b_func = extract_json_string(body, "alg_b_func", "");
    int alg_b_type = extract_json_int(body, "alg_b_type", -1);

    // Fallbacks if nested in algorithms array or not provided
    if (alg_a_path.empty()) {
        size_t pos_alg = body.find("\"source_path\":");
        if (pos_alg != std::string::npos) {
            size_t q1 = body.find('"', pos_alg + 14);
            if (q1 != std::string::npos) {
                size_t q2 = body.find('"', q1 + 1);
                if (q2 != std::string::npos) {
                    alg_a_path = body.substr(q1 + 1, q2 - q1 - 1);
                }
            }
        }
    }
    if (alg_a_path.empty()) alg_a_path = "custom/samples/linear_search.cpp";

    if (alg_b_path.empty()) {
        size_t pos_alg = body.find("\"source_path\":");
        if (pos_alg != std::string::npos) {
            size_t pos_second = body.find("\"source_path\":", pos_alg + 14);
            if (pos_second != std::string::npos) {
                size_t q1 = body.find('"', pos_second + 14);
                if (q1 != std::string::npos) {
                    size_t q2 = body.find('"', q1 + 1);
                    if (q2 != std::string::npos) {
                        alg_b_path = body.substr(q1 + 1, q2 - q1 - 1);
                    }
                }
            }
        }
    }
    if (alg_b_path.empty()) alg_b_path = "custom/samples/binary_search.cpp";

    // Auto-detect interfaces for alg A
    if (alg_a_func.empty() || alg_a_type < 0) {
        auto det = custom::InterfaceDetector::detect_from_file(alg_a_path, custom::CustomCategory::Search);
        if (det.recognized) {
            alg_a_func = det.detected_function_name;
            alg_a_type = static_cast<int>(det.interface_type);
            if (alg_a_name.empty()) alg_a_name = det.suggested_display_name;
        } else {
            alg_a_func = "linear_search";
            alg_a_type = static_cast<int>(custom::SearchInterfaceType::VectorRefTarget);
        }
    }
    if (alg_a_name.empty()) alg_a_name = "Linear Search";

    // Auto-detect interfaces for alg B
    if (alg_b_func.empty() || alg_b_type < 0) {
        auto det = custom::InterfaceDetector::detect_from_file(alg_b_path, custom::CustomCategory::Search);
        if (det.recognized) {
            alg_b_func = det.detected_function_name;
            alg_b_type = static_cast<int>(det.interface_type);
            if (alg_b_name.empty()) alg_b_name = det.suggested_display_name;
        } else {
            alg_b_func = "binarySearch";
            alg_b_type = static_cast<int>(custom::SearchInterfaceType::PointerSizeTarget);
        }
    }
    if (alg_b_name.empty()) alg_b_name = "Binary Search";

    custom::AlgorithmSourceSpec a1;
    a1.algorithm_name = alg_a_name;
    a1.source_file_path = alg_a_path;
    a1.detected_function = alg_a_func;
    a1.interface_type = static_cast<custom::SearchInterfaceType>(alg_a_type);
    cfg.algorithms.push_back(a1);

    custom::AlgorithmSourceSpec a2;
    a2.algorithm_name = alg_b_name;
    a2.source_file_path = alg_b_path;
    a2.detected_function = alg_b_func;
    a2.interface_type = static_cast<custom::SearchInterfaceType>(alg_b_type);
    cfg.algorithms.push_back(a2);

    cancel_requested.store(false);

    {
        std::lock_guard<std::mutex> lock(status_mutex);
        current_progress.state = EngineState::Running;
        current_progress.progress_percent = 10;
        current_progress.current_algorithm = "Compiling custom algorithms...";
        current_progress.current_input_type = "Custom Search";
        current_progress.current_size = 0;
        current_progress.current_iteration = 0;
        current_progress.total_iterations = cfg.iterations;
        current_progress.elapsed_seconds = 0.0;
        current_progress.error_message = "";
    }

    if (worker_thread && worker_thread->joinable()) {
        worker_thread->join();
    }

    worker_thread = std::make_unique<std::thread>([this, cfg]() {
        try {
            {
                std::lock_guard<std::mutex> lock(status_mutex);
                current_progress.progress_percent = 35;
                current_progress.current_algorithm = "Running isolated benchmark process...";
            }

            auto res = custom::CustomBenchmarkRunner::execute(cfg);

            std::lock_guard<std::mutex> lock(status_mutex);
            if (res.success) {
                current_progress.state = EngineState::Completed;
                current_progress.progress_percent = 100;
                current_progress.last_run_id = res.run.run_id;
                current_progress.current_algorithm = "Completed ✓";
            } else {
                current_progress.state = EngineState::Failed;
                current_progress.error_message = res.error_message;
            }
        } catch (const std::exception& ex) {
            std::lock_guard<std::mutex> lock(status_mutex);
            current_progress.state = EngineState::Failed;
            current_progress.error_message = ex.what();
        }
    });

    return "{\"status\": \"started\", \"message\": \"Custom benchmark started\"}";
}

std::string DashboardServer::handle_cancel() {
    cancel_requested.store(true);
    {
        std::lock_guard<std::mutex> lock(status_mutex);
        if (current_progress.state == EngineState::Running) {
            current_progress.current_algorithm = "Cancelling...";
        }
    }
    return "{\"status\": \"cancelling\"}";
}

std::string DashboardServer::handle_report(const std::string& query_id) {
    analysis::HistoryManager history;
    analysis::LoadResult res;
    if (query_id.empty()) {
        auto latest = history.get_latest_run();
        if (!latest.has_value()) return "{\"error\": \"No runs in history\"}";
        res.run = *latest;
    } else {
        res = history.load_run(query_id);
        if (!res.success) return "{\"error\": \"" + escape_json_str(res.error_message) + "\"}";
    }

    auto metas = history.list_runs();
    std::vector<analysis::BenchmarkRun> all_runs;
    for (const auto& m : metas) {
        auto r = history.load_run(m.run_id);
        if (r.success) all_runs.push_back(r.run);
    }

    reporting::ReportOptions opts;
    opts.auto_open_in_browser = true;
    std::string path = reporting::HtmlReportGenerator::generate_from_run(res.run, all_runs, opts);
    return "{\"success\": true, \"report_path\": \"" + escape_json_str(path) + "\"}";
}

bool DashboardServer::serve_static_file(uintptr_t sock_ptr, const std::string& raw_path) {
    std::string clean_path = raw_path;
    if (clean_path.empty() || clean_path == "/") {
        clean_path = "/index.html";
    }

    if (clean_path.find("..") != std::string::npos) {
        return false;
    }

    std::filesystem::path full_path = std::filesystem::path(web_root) / clean_path.substr(1);
    if (!std::filesystem::exists(full_path) || std::filesystem::is_directory(full_path)) {
        return false;
    }

    std::ifstream file(full_path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    std::string content = ss.str();

    std::string mime = get_mime_type(full_path.string());
    send_response(sock_ptr, 200, mime, content);
    return true;
}

std::string DashboardServer::get_mime_type(const std::string& path) {
    std::string ext;
    size_t dot = path.rfind('.');
    if (dot != std::string::npos) ext = path.substr(dot);

    if (ext == ".html") return "text/html; charset=utf-8";
    if (ext == ".css")  return "text/css; charset=utf-8";
    if (ext == ".js")   return "application/javascript; charset=utf-8";
    if (ext == ".json") return "application/json; charset=utf-8";
    if (ext == ".svg")  return "image/svg+xml";
    if (ext == ".png")  return "image/png";
    if (ext == ".ico")  return "image/x-icon";
    return "application/octet-stream";
}

void DashboardServer::send_response(uintptr_t sock_ptr, int status_code, const std::string& content_type, const std::string& body) {
    SOCKET client_sock = static_cast<SOCKET>(sock_ptr);

    std::string status_text = "OK";
    if (status_code == 204) status_text = "No Content";
    else if (status_code == 404) status_text = "Not Found";
    else if (status_code == 500) status_text = "Internal Server Error";

    std::ostringstream ss;
    ss << "HTTP/1.1 " << status_code << " " << status_text << "\r\n";
    ss << "Content-Type: " << content_type << "\r\n";
    ss << "Content-Length: " << body.size() << "\r\n";
    ss << "Access-Control-Allow-Origin: *\r\n";
    ss << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
    ss << "Access-Control-Allow-Headers: Content-Type\r\n";
    ss << "Connection: close\r\n\r\n";
    ss << body;

    std::string resp = ss.str();
    const char* ptr = resp.c_str();
    int remaining = static_cast<int>(resp.size());
    while (remaining > 0) {
        int sent = send(client_sock, ptr, remaining, 0);
        if (sent <= 0) {
            break;
        }
        ptr += sent;
        remaining -= sent;
    }

#ifdef _WIN32
    shutdown(client_sock, SD_SEND);
#else
    shutdown(client_sock, SHUT_WR);
#endif
}

std::map<std::string, std::string> DashboardServer::parse_query(const std::string& query_str) {
    std::map<std::string, std::string> result;
    std::istringstream stream(query_str);
    std::string pair;
    while (std::getline(stream, pair, '&')) {
        size_t eq = pair.find('=');
        if (eq != std::string::npos) {
            result[pair.substr(0, eq)] = pair.substr(eq + 1);
        } else if (!pair.empty()) {
            result[pair] = "";
        }
    }
    return result;
}

} // namespace server

