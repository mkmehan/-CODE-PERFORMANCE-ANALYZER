#include "CustomAlgorithm.h"
#include "../HighResolutionTimer.h"
#include "../RDTSC_Timer.h"
#include "../MemoryMonitor.h"
#include "../Statistics.h"
#include "../CpuAffinity.h"
#include "../SystemInfo.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <iomanip>
#include <chrono>
#include <ctime>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif

// Defined by the synthesized adapter translation unit
namespace custom {
std::vector<std::unique_ptr<ISearchAlgorithm>> create_custom_algorithms();
}

namespace {

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

std::string current_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
#ifdef _WIN32
    localtime_s(&tm_buf, &tt);
#else
    localtime_r(&tt, &tm_buf);
#endif
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d_%H-%M-%S", &tm_buf);
    return std::string(buf);
}

} // namespace

int main(int argc, char* argv[]) {
    std::string dataset_path = "custom/samples/search_data.txt";
    int target_value = 5000;
    int iterations = 20;
    int warmup_runs = 5;
    int cpu_affinity = -1;
    bool measure_memory = true;
    std::string json_output_path;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--dataset" && i + 1 < argc) dataset_path = argv[++i];
        else if (arg == "--target" && i + 1 < argc) target_value = std::stoi(argv[++i]);
        else if (arg == "--iterations" && i + 1 < argc) iterations = std::stoi(argv[++i]);
        else if (arg == "--warmup" && i + 1 < argc) warmup_runs = std::stoi(argv[++i]);
        else if (arg == "--affinity" && i + 1 < argc) cpu_affinity = std::stoi(argv[++i]);
        else if (arg == "--memory" && i + 1 < argc) measure_memory = (std::string(argv[++i]) == "true");
        else if (arg == "--json-output" && i + 1 < argc) json_output_path = argv[++i];
    }

    if (iterations < 1) iterations = 1;
    if (warmup_runs < 0) warmup_runs = 0;

    // 1. Load dataset
    std::ifstream infile(dataset_path);
    if (!infile.is_open()) {
        std::cerr << "{\"status\": \"error\", \"error_message\": \"Failed to open dataset file: "
                  << escape_json_str(dataset_path) << "\"}\n";
        return 1;
    }

    std::vector<int> data;
    int val = 0;
    while (infile >> val) {
        data.push_back(val);
    }
    infile.close();

    if (data.empty()) {
        std::cerr << "{\"status\": \"error\", \"error_message\": \"Dataset file is empty: "
                  << escape_json_str(dataset_path) << "\"}\n";
        return 2;
    }

    // Determine expected index for ground-truth verification
    int expected_index = -1;
    for (size_t i = 0; i < data.size(); ++i) {
        if (data[i] == target_value) {
            expected_index = static_cast<int>(i);
            break;
        }
    }

    // 2. Instantiate custom algorithms
    auto algorithms = custom::create_custom_algorithms();
    if (algorithms.empty()) {
        std::cerr << "{\"status\": \"error\", \"error_message\": \"No custom algorithms were registered to benchmark\"}\n";
        return 3;
    }

    // Prepare each algorithm
    for (auto& alg : algorithms) {
        alg->prepare(data.data(), data.size());
    }

    // 3. Set CPU affinity if requested
    std::unique_ptr<CpuAffinityGuard> affinity_guard;
    if (cpu_affinity >= 0) {
        affinity_guard = std::make_unique<CpuAffinityGuard>(cpu_affinity);
    }

    RDTSC_Timer overhead_timer;
    overhead_timer.start();
    overhead_timer.stop();
    uint64_t rdtsc_overhead = overhead_timer.elapsed();
    SystemInfo sys;

    // 4. Benchmark execution
    struct ResultSummary {
        std::string name;
        bool verified = false;
        double time_min = 0.0;
        double time_max = 0.0;
        double time_mean = 0.0;
        double time_median = 0.0;
        double time_stddev = 0.0;
        double cycles_min = 0.0;
        double cycles_max = 0.0;
        double cycles_mean = 0.0;
        double cycles_median = 0.0;
        double cycles_stddev = 0.0;
        uint64_t peak_mem_increase = 0;
        int64_t net_mem_change = 0;
    };

    std::vector<ResultSummary> results;

    for (size_t a_idx = 0; a_idx < algorithms.size(); ++a_idx) {
        auto& alg = algorithms[a_idx];

        // Warmup runs
        for (int w = 0; w < warmup_runs; ++w) {
            volatile int sink = alg->search(data.data(), data.size(), target_value);
            (void)sink;
        }

        // Verification check
        int found_idx = alg->search(data.data(), data.size(), target_value);
        bool is_verified = (found_idx == expected_index);

        std::vector<uint64_t> time_samples;
        std::vector<uint64_t> cycle_samples;
        time_samples.reserve(iterations);
        cycle_samples.reserve(iterations);

        uint64_t max_peak_increase = 0;
        int64_t total_net_change = 0;

        HighResolutionTimer hr_timer;
        RDTSC_Timer rdtsc_timer;

        for (int it = 0; it < iterations; ++it) {
            MemorySnapshot before_mem{};
            if (measure_memory) {
                before_mem = MemoryMonitor::snapshot();
            }

            rdtsc_timer.start();
            hr_timer.start();

            volatile int res = alg->search(data.data(), data.size(), target_value);

            hr_timer.stop();
            rdtsc_timer.stop();
            (void)res;

            uint64_t time_ns = hr_timer.elapsed();
            uint64_t raw_cycles = rdtsc_timer.elapsed();
            uint64_t net_cycles = (raw_cycles > rdtsc_overhead) ? (raw_cycles - rdtsc_overhead) : raw_cycles;

            time_samples.push_back(time_ns);
            cycle_samples.push_back(net_cycles);

            if (measure_memory) {
                MemorySnapshot after_mem = MemoryMonitor::snapshot();
                uint64_t peak = (after_mem.private_bytes > before_mem.private_bytes) ? (after_mem.private_bytes - before_mem.private_bytes) : 0;
                if (peak > max_peak_increase) max_peak_increase = peak;
                total_net_change += (static_cast<int64_t>(after_mem.private_bytes) - static_cast<int64_t>(before_mem.private_bytes));
            }
        }

        ResultSummary rs;
        rs.name = alg->name();
        rs.verified = is_verified;
        rs.time_min = static_cast<double>(get_minimum(time_samples));
        rs.time_max = static_cast<double>(get_maximum(time_samples));
        rs.time_mean = get_average(time_samples);
        rs.time_median = get_median(time_samples);
        rs.time_stddev = get_standard_deviation(time_samples, rs.time_mean);
        rs.cycles_min = static_cast<double>(get_minimum(cycle_samples));
        rs.cycles_max = static_cast<double>(get_maximum(cycle_samples));
        rs.cycles_mean = get_average(cycle_samples);
        rs.cycles_median = get_median(cycle_samples);
        rs.cycles_stddev = get_standard_deviation(cycle_samples, rs.cycles_mean);
        rs.peak_mem_increase = max_peak_increase;
        rs.net_mem_change = (iterations > 0) ? (total_net_change / iterations) : 0;

        results.push_back(rs);
    }

    // 5. Serialize BenchmarkRun JSON
    std::string ts = current_timestamp();
    std::string run_id = "RUN-" + ts;

    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"status\": \"success\",\n";
    ss << "  \"run\": {\n";
    ss << "    \"format_version\": \"4.0\",\n";
    ss << "    \"run_id\": \"" << run_id << "\",\n";
    ss << "    \"timestamp\": \"" << ts << "\",\n";
    ss << "    \"benchmark_mode\": \"custom\",\n";
    ss << "    \"benchmark_category\": \"search\",\n";
    ss << "    \"custom_target_parameter\": \"Target: " << target_value << " (Index: " << expected_index << ")\",\n";
    ss << "    \"system\": {\n";
    ss << "      \"os\": \"" << escape_json_str(sys.operating_system()) << "\",\n";
    ss << "      \"cpu\": \"" << escape_json_str(sys.cpu_name()) << "\",\n";
    ss << "      \"architecture\": \"" << escape_json_str(sys.architecture()) << "\",\n";
    ss << "      \"physical_cores\": " << sys.physical_cores() << ",\n";
    ss << "      \"logical_cpus\": " << sys.logical_processors() << ",\n";
    ss << "      \"compiler\": \"" << escape_json_str(sys.compiler()) << "\",\n";
    ss << "      \"cxx_standard\": \"" << escape_json_str(sys.cxx_standard()) << "\",\n";
    ss << "      \"optimization\": \"" << escape_json_str(sys.optimization()) << "\"\n";
    ss << "    },\n";
    ss << "    \"configuration\": {\n";
    ss << "      \"warmup_runs\": " << warmup_runs << ",\n";
    ss << "      \"iterations\": " << iterations << ",\n";
    ss << "      \"random_seed\": 0,\n";
    ss << "      \"use_rdtsc\": true,\n";
    ss << "      \"use_high_resolution_timer\": true,\n";
    ss << "      \"measure_memory\": " << (measure_memory ? "true" : "false") << ",\n";
    ss << "      \"cpu_affinity\": " << cpu_affinity << ",\n";
    ss << "      \"input_sizes\": [" << data.size() << "],\n";
    ss << "      \"input_cases\": [\"CustomFile\"]\n";
    ss << "    },\n";
    ss << "    \"input\": {\n";
    ss << "      \"type\": \"custom_file\",\n";
    ss << "      \"file_path\": \"" << escape_json_str(dataset_path) << "\",\n";
    ss << "      \"element_count\": " << data.size() << ",\n";
    ss << "      \"distinct_count\": " << data.size() << ",\n";
    ss << "      \"duplicate_count\": 0,\n";
    ss << "      \"min_value\": " << (data.empty() ? 0 : data.front()) << ",\n";
    ss << "      \"max_value\": " << (data.empty() ? 0 : data.back()) << ",\n";
    ss << "      \"order_description\": \"Custom Search Dataset\"\n";
    ss << "    },\n";
    ss << "    \"results\": [\n";

    for (size_t i = 0; i < results.size(); ++i) {
        const auto& r = results[i];
        std::string key = r.name;
        std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) {
            return (c == ' ' || c == '-') ? '_' : std::tolower(c);
        });

        ss << "      {\n";
        ss << "        \"algorithm\": \"" << escape_json_str(key) << "\",\n";
        ss << "        \"algorithm_name\": \"" << escape_json_str(r.name) << "\",\n";
        ss << "        \"input_type\": \"Search Dataset\",\n";
        ss << "        \"input_size\": " << data.size() << ",\n";
        ss << "        \"verified\": " << (r.verified ? "true" : "false") << ",\n";
        ss << "        \"time_min_ns\": " << std::fixed << std::setprecision(0) << r.time_min << ",\n";
        ss << "        \"time_max_ns\": " << std::fixed << std::setprecision(0) << r.time_max << ",\n";
        ss << "        \"time_mean_ns\": " << std::fixed << std::setprecision(0) << r.time_mean << ",\n";
        ss << "        \"time_median_ns\": " << std::fixed << std::setprecision(0) << r.time_median << ",\n";
        ss << "        \"time_stddev_ns\": " << std::fixed << std::setprecision(2) << r.time_stddev << ",\n";
        ss << "        \"cycles_min\": " << std::fixed << std::setprecision(0) << r.cycles_min << ",\n";
        ss << "        \"cycles_max\": " << std::fixed << std::setprecision(0) << r.cycles_max << ",\n";
        ss << "        \"cycles_mean\": " << std::fixed << std::setprecision(0) << r.cycles_mean << ",\n";
        ss << "        \"cycles_median\": " << std::fixed << std::setprecision(0) << r.cycles_median << ",\n";
        ss << "        \"cycles_stddev\": " << std::fixed << std::setprecision(2) << r.cycles_stddev << ",\n";
        ss << "        \"memory_peak_increase_bytes\": " << r.peak_mem_increase << ",\n";
        ss << "        \"memory_net_change_bytes\": " << r.net_mem_change << "\n";
        ss << "      }" << (i + 1 < results.size() ? "," : "") << "\n";
    }

    ss << "    ]\n";
    ss << "  }\n";
    ss << "}\n";

    std::string out_json = ss.str();
    if (!json_output_path.empty()) {
        std::ofstream outfile(json_output_path);
        if (outfile.is_open()) {
            outfile << out_json;
            outfile.close();
        }
    }

    std::cout << out_json << std::flush;
    return 0;
}

