#ifndef SERVER_DASHBOARD_SERVER_H
#define SERVER_DASHBOARD_SERVER_H

#include "../analysis/BenchmarkRun.h"
#include "../analysis/HistoryManager.h"
#include "../analysis/ComparisonAnalyzer.h"
#include "../analysis/RegressionAnalyzer.h"
#include "../analysis/TrendAnalyzer.h"
#include "../FileInputLoader.h"
#include "../BenchmarkRunner.h"

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace server {

enum class EngineState {
    Idle,
    Running,
    Completed,
    Cancelled,
    Failed
};

struct BenchmarkProgress {
    EngineState state = EngineState::Idle;
    int progress_percent = 0;          // 0 to 100
    std::string current_algorithm;
    std::string current_input_type;
    size_t current_size = 0;
    int current_iteration = 0;
    int total_iterations = 0;
    double elapsed_seconds = 0.0;
    std::string last_run_id;
    std::string error_message;
};

class DashboardServer {
public:
    explicit DashboardServer(int port = 8080, std::string web_root = "web");
    ~DashboardServer();

    // Start server on localhost (tries initial port, falls back to port+1)
    bool start();

    // Stop server and join listener threads
    void stop();

    // Block calling thread until server stops
    void wait();

    bool is_running() const { return running.load(); }
    int port() const { return active_port; }
    std::string url() const;

    // Direct status query
    BenchmarkProgress get_progress() const;

private:
    int requested_port;
    int active_port = 0;
    std::string web_root;

    std::atomic<bool> running{false};
    std::atomic<bool> cancel_requested{false};

    mutable std::mutex status_mutex;
    BenchmarkProgress current_progress;

    std::unique_ptr<std::thread> listener_thread;
    std::unique_ptr<std::thread> worker_thread;

    uintptr_t server_socket = 0; // SOCKET representation

    void run_listener();
    void handle_client(uintptr_t client_sock);

    // Modular request handlers
    std::string handle_status();
    std::string handle_history();
    std::string handle_run(const std::string& query_id);
    std::string handle_compare(const std::string& query_id);
    std::string handle_regression(const std::string& query_id);
    std::string handle_trend(const std::string& metric, const std::string& distribution);
    std::string handle_validate_file(const std::string& body);
    std::string handle_benchmark(const std::string& body);
    std::string handle_cancel();
    std::string handle_report(const std::string& query_id);
    bool serve_static_file(uintptr_t client_sock, const std::string& raw_path);

    // Helper utilities
    static std::string get_mime_type(const std::string& path);
    static void send_response(uintptr_t client_sock, int status_code, const std::string& content_type, const std::string& body);
    static std::map<std::string, std::string> parse_query(const std::string& query_str);
};

} // namespace server

#endif // SERVER_DASHBOARD_SERVER_H
