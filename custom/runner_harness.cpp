#include "CustomAlgorithm.h"
#include "../HighResolutionTimer.h"
#include "../RDTSC_Timer.h"
#include "../MemoryMonitor.h"
#include "../Statistics.h"
#include "../CpuAffinity.h"
#include "../SystemInfo.h"
#include "../ComplexityAnalyzer.h"

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

    const size_t total_elements = data.size();

    // 2. Scan dataset for target availability and occurrences
    bool target_available = false;
    size_t occurrences = 0;
    int first_expected_index = -1;

    for (size_t i = 0; i < total_elements; ++i) {
        if (data[i] == target_value) {
            if (!target_available) {
                target_available = true;
                first_expected_index = static_cast<int>(i);
            }
            occurrences++;
        }
    }

    std::string mode_description = target_available
        ? "Present-target benchmark (target present in dataset)"
        : "Absent-target negative test (target absent from dataset)";

    // 3. Build 5 safe subset sizes for multi-size empirical complexity analysis
    std::vector<size_t> test_sizes;
    if (total_elements >= 5) {
        for (int k = 1; k <= 5; ++k) {
            size_t sz = std::max<size_t>(static_cast<size_t>(k), (total_elements * k) / 5);
            if (test_sizes.empty() || sz > test_sizes.back()) {
                test_sizes.push_back(sz);
            }
        }
    } else {
        test_sizes.push_back(total_elements);
    }

    // 4. Instantiate custom algorithms
    auto algorithms = custom::create_custom_algorithms();
    if (algorithms.empty()) {
        std::cerr << "{\"status\": \"error\", \"error_message\": \"No custom algorithms were registered to benchmark\"}\n";
        return 3;
    }

    // 5. Set CPU affinity if requested
    std::unique_ptr<CpuAffinityGuard> affinity_guard;
    if (cpu_affinity >= 0) {
        affinity_guard = std::make_unique<CpuAffinityGuard>(cpu_affinity);
    }

    RDTSC_Timer overhead_timer;
    overhead_timer.start();
    overhead_timer.stop();
    uint64_t rdtsc_overhead = overhead_timer.elapsed();
    SystemInfo sys;

    // Measurement record struct
    struct MeasurementRecord {
        std::string name;
        size_t size = 0;
        bool verified = false;
        int search_result_index = -1;
        bool search_target_found = false;
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

    std::vector<std::vector<MeasurementRecord>> all_alg_measurements(algorithms.size());

    // 6. Benchmark execution across each subset size
    for (size_t s_idx = 0; s_idx < test_sizes.size(); ++s_idx) {
        size_t sz = test_sizes[s_idx];

        // Determine data slice:
        // In present-target mode, if sz is smaller than first_expected_index,
        // take a slice ending at first_expected_index so target is guaranteed present in slice.
        // If sz contains first_expected_index or target is absent, take standard prefix slice.
        const int* slice_data = data.data();
        bool target_in_slice = false;

        if (target_available) {
            if (first_expected_index < static_cast<int>(sz)) {
                slice_data = data.data();
                target_in_slice = true;
            } else {
                size_t offset = static_cast<size_t>(first_expected_index) - sz + 1;
                slice_data = data.data() + offset;
                target_in_slice = true;
            }
        } else {
            slice_data = data.data();
            target_in_slice = false;
        }

        for (size_t a_idx = 0; a_idx < algorithms.size(); ++a_idx) {
            auto& alg = algorithms[a_idx];

            // Prepare algorithm for this exact slice size
            alg->prepare(slice_data, sz);

            // Warmup runs
            for (int w = 0; w < warmup_runs; ++w) {
                volatile int sink = alg->search(slice_data, sz, target_value);
                (void)sink;
            }

            // Verification check with robust duplicate value support:
            // - If returned -1: valid only when target is absent from slice
            // - If returned >= 0: valid when in range [0, sz) AND slice_data[index] == target
            int found_idx = alg->search(slice_data, sz, target_value);
            bool is_verified = false;
            bool target_found = (found_idx >= 0);

            if (found_idx == -1) {
                is_verified = !target_in_slice;
            } else if (found_idx >= 0 && static_cast<size_t>(found_idx) < sz) {
                is_verified = (slice_data[found_idx] == target_value);
            }

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

                volatile int res = alg->search(slice_data, sz, target_value);

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

            MeasurementRecord rec;
            rec.name = alg->name();
            rec.size = sz;
            rec.verified = is_verified;
            rec.search_result_index = found_idx;
            rec.search_target_found = target_found;
            rec.time_min = static_cast<double>(get_minimum(time_samples));
            rec.time_max = static_cast<double>(get_maximum(time_samples));
            rec.time_mean = get_average(time_samples);
            rec.time_median = get_median(time_samples);
            rec.time_stddev = get_standard_deviation(time_samples, rec.time_mean);
            rec.cycles_min = static_cast<double>(get_minimum(cycle_samples));
            rec.cycles_max = static_cast<double>(get_maximum(cycle_samples));
            rec.cycles_mean = get_average(cycle_samples);
            rec.cycles_median = get_median(cycle_samples);
            rec.cycles_stddev = get_standard_deviation(cycle_samples, rec.cycles_mean);
            rec.peak_mem_increase = max_peak_increase;
            rec.net_mem_change = (iterations > 0) ? (total_net_change / iterations) : 0;

            all_alg_measurements[a_idx].push_back(rec);
        }
    }

    // 7. Complexity Analysis per Algorithm
    struct ComplexitySummary {
        std::string algorithm;
        std::string algorithm_name;
        std::string theoretical;
        std::string observed_model;
        std::string observed_complexity;
        double fit_quality = 0.0;
    };

    std::vector<ComplexitySummary> complexity_summaries;
    ComplexityAnalyzer comp_analyzer;

    for (size_t a_idx = 0; a_idx < algorithms.size(); ++a_idx) {
        const auto& records = all_alg_measurements[a_idx];
        std::vector<size_t> sizes_for_fit;
        std::vector<double> times_for_fit;
        std::vector<double> cycles_for_fit;

        for (const auto& r : records) {
            sizes_for_fit.push_back(r.size);
            times_for_fit.push_back(r.time_mean);
            cycles_for_fit.push_back(r.cycles_mean);
        }

        ComplexityResult c_res = comp_analyzer.analyze(sizes_for_fit, times_for_fit);
        // Fallback to cycles if times are in sub-microsecond zero-quantization range
        if (c_res.sample_count < 3 || c_res.fit_quality < 0.2) {
            ComplexityResult c_cycles = comp_analyzer.analyze(sizes_for_fit, cycles_for_fit);
            if (c_cycles.sample_count >= 3 && c_cycles.fit_quality >= c_res.fit_quality) {
                c_res = c_cycles;
            }
        }

        std::string alg_name = algorithms[a_idx]->name();
        std::string lower_name = alg_name;
        std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), [](unsigned char c){ return std::tolower(c); });

        std::string theoretical = "O(n)";
        if (lower_name.find("binary") != std::string::npos) {
            theoretical = "O(log n)";
        }

        std::string observed_complexity = "Indeterminate";
        if (c_res.sample_count >= 3) {
            if (c_res.exponent < 0.35) {
                observed_complexity = "O(log n) - Sub-linear";
            } else if (c_res.exponent >= 0.70 && c_res.exponent <= 1.35) {
                observed_complexity = "O(n) - Linear";
            } else if (c_res.exponent > 1.35 && c_res.exponent <= 1.75) {
                observed_complexity = "O(n log n)";
            } else if (c_res.exponent > 1.75) {
                observed_complexity = "O(n^2) - Quadratic";
            } else {
                std::ostringstream ss;
                ss << "O(n^" << std::fixed << std::setprecision(2) << c_res.exponent << ")";
                observed_complexity = ss.str();
            }
        } else {
            observed_complexity = theoretical + " (Insufficient data for fit)";
        }

        std::string key = alg_name;
        std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) {
            return (c == ' ' || c == '-') ? '_' : std::tolower(c);
        });

        ComplexitySummary cs;
        cs.algorithm = key;
        cs.algorithm_name = alg_name;
        cs.theoretical = theoretical;
        cs.observed_model = c_res.observed_model;
        cs.observed_complexity = observed_complexity;
        cs.fit_quality = c_res.fit_quality;

        complexity_summaries.push_back(cs);
    }

    // 8. Serialize BenchmarkRun JSON
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
    ss << "    \"custom_target_parameter\": \"Target: " << target_value
       << (target_available ? (" (Found @ Index: " + std::to_string(first_expected_index) + ")") : " (Not present in dataset)") << "\",\n";

    // Target detection metadata
    ss << "    \"target_detection\": {\n";
    ss << "      \"target_value\": " << target_value << ",\n";
    ss << "      \"available_in_dataset\": " << (target_available ? "true" : "false") << ",\n";
    ss << "      \"occurrences\": " << occurrences << ",\n";
    ss << "      \"expected_index\": " << first_expected_index << ",\n";
    ss << "      \"mode_description\": \"" << escape_json_str(mode_description) << "\"\n";
    ss << "    },\n";

    // Complexity metadata
    ss << "    \"complexity\": [\n";
    for (size_t i = 0; i < complexity_summaries.size(); ++i) {
        const auto& c = complexity_summaries[i];
        ss << "      {\n";
        ss << "        \"algorithm\": \"" << escape_json_str(c.algorithm) << "\",\n";
        ss << "        \"algorithm_name\": \"" << escape_json_str(c.algorithm_name) << "\",\n";
        ss << "        \"theoretical\": \"" << escape_json_str(c.theoretical) << "\",\n";
        ss << "        \"observed_model\": \"" << escape_json_str(c.observed_model) << "\",\n";
        ss << "        \"observed_complexity\": \"" << escape_json_str(c.observed_complexity) << "\",\n";
        ss << "        \"fit_quality\": " << std::fixed << std::setprecision(2) << c.fit_quality << "\n";
        ss << "      }" << (i + 1 < complexity_summaries.size() ? "," : "") << "\n";
    }
    ss << "    ],\n";

    // System info
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

    // Configuration
    ss << "    \"configuration\": {\n";
    ss << "      \"warmup_runs\": " << warmup_runs << ",\n";
    ss << "      \"iterations\": " << iterations << ",\n";
    ss << "      \"random_seed\": 0,\n";
    ss << "      \"use_rdtsc\": true,\n";
    ss << "      \"use_high_resolution_timer\": true,\n";
    ss << "      \"measure_memory\": " << (measure_memory ? "true" : "false") << ",\n";
    ss << "      \"cpu_affinity\": " << cpu_affinity << ",\n";
    ss << "      \"input_sizes\": [";
    for (size_t i = 0; i < test_sizes.size(); ++i) {
        ss << (i == 0 ? "" : ", ") << test_sizes[i];
    }
    ss << "],\n";
    ss << "      \"input_cases\": [\"SearchDataset\"]\n";
    ss << "    },\n";

    // Input metadata
    ss << "    \"input\": {\n";
    ss << "      \"type\": \"custom_file\",\n";
    ss << "      \"file_path\": \"" << escape_json_str(dataset_path) << "\",\n";
    ss << "      \"element_count\": " << total_elements << ",\n";
    ss << "      \"distinct_count\": " << total_elements << ",\n";
    ss << "      \"duplicate_count\": 0,\n";
    ss << "      \"min_value\": " << (data.empty() ? 0 : data.front()) << ",\n";
    ss << "      \"max_value\": " << (data.empty() ? 0 : data.back()) << ",\n";
    ss << "      \"order_description\": \"" << escape_json_str(mode_description) << "\"\n";
    ss << "    },\n";

    // Results array across all algorithms and all sizes
    ss << "    \"results\": [\n";
    bool first_record = true;
    for (size_t a_idx = 0; a_idx < algorithms.size(); ++a_idx) {
        for (size_t s_idx = 0; s_idx < all_alg_measurements[a_idx].size(); ++s_idx) {
            const auto& r = all_alg_measurements[a_idx][s_idx];
            std::string key = r.name;
            std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) {
                return (c == ' ' || c == '-') ? '_' : std::tolower(c);
            });

            if (!first_record) ss << ",\n";
            first_record = false;

            ss << "      {\n";
            ss << "        \"algorithm\": \"" << escape_json_str(key) << "\",\n";
            ss << "        \"algorithm_name\": \"" << escape_json_str(r.name) << "\",\n";
            ss << "        \"input_type\": \"Search Dataset\",\n";
            ss << "        \"input_size\": " << r.size << ",\n";
            ss << "        \"verified\": " << (r.verified ? "true" : "false") << ",\n";
            ss << "        \"search_result_index\": " << r.search_result_index << ",\n";
            ss << "        \"search_target_found\": " << (r.search_target_found ? "true" : "false") << ",\n";
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
            ss << "      }";
        }
    }
    ss << "\n    ]\n";
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
