#ifndef ANALYSIS_BENCHMARK_RUN_H
#define ANALYSIS_BENCHMARK_RUN_H

#include "ResultModel.h"

#include <string>
#include <vector>
#include <cstddef>
#include <cstdint>

namespace analysis {

struct InputMetadata {
    std::string type;               // "custom_file" or "generated"
    std::string file_path;          // Relative or absolute path if custom_file
    size_t element_count = 0;
    size_t distinct_count = 0;
    size_t duplicate_count = 0;
    int min_value = 0;
    int max_value = 0;
    std::string order_description;  // e.g. "Sorted", "Reverse Sorted", "All Equal", "Unsorted"
};

struct SystemMetadata {
    std::string os;
    std::string cpu;
    std::string architecture;
    unsigned int physical_cores = 0;
    unsigned int logical_cpus = 0;
    std::string compiler;
    std::string cxx_standard;
    std::string optimization;
};

struct ConfigMetadata {
    int warmup_runs = 0;
    int iterations = 0;
    uint32_t random_seed = 0;
    bool use_rdtsc = true;
    bool use_high_resolution_timer = true;
    bool measure_memory = true;
    int cpu_affinity = -1;
    std::vector<size_t> input_sizes;
    std::vector<std::string> input_cases;
};

struct BenchmarkRun {
    std::string format_version = "4.0";
    std::string run_id;            // e.g. "RUN-20260913-024701"
    std::string timestamp;         // e.g. "2026-09-13_02-47-01"
    std::string benchmark_mode = "standard";    // "standard" or "custom"
    std::string benchmark_category = "sorting"; // "sorting", "search", "matrix", etc.
    std::string custom_target_parameter;        // e.g. "Target: 5000"
    SystemMetadata system;
    ConfigMetadata configuration;
    InputMetadata input;
    std::vector<BenchmarkRecord> results;
};

} // namespace analysis

#endif // ANALYSIS_BENCHMARK_RUN_H

