#ifndef CUSTOM_BENCHMARK_RUNNER_H
#define CUSTOM_BENCHMARK_RUNNER_H

#include "../analysis/BenchmarkRun.h"
#include "CustomBenchmarkCompiler.h"

#include <atomic>
#include <string>
#include <vector>

namespace custom {

struct SearchConfig {
    std::string dataset = "custom/samples/search_data.txt";
    bool has_target = true;
    int target = 5000;
};

struct MatrixConfig {
    int matrix_size = 128;
    std::string matrix_a;
    std::string matrix_b;
    std::string operation = "multiply";
};

struct GraphConfig {
    std::string graph_dataset;
    int vertices = 100;
    int edges = 500;
    int source = 0;
};

struct SortingConfig {
    std::string dataset;
};

struct CustomBenchmarkConfig {
    std::string category = "search";
    std::string interface_mode = "with_target"; // "with_target" or "dataset_only"
    bool has_target = true;
    std::string dataset_path = "custom/samples/search_data.txt";
    int target_value = 5000;
    int iterations = 20;
    int warmup_runs = 5;
    int cpu_affinity = -1;
    bool measure_memory = true;
    int timeout_seconds = 30;
    std::vector<AlgorithmSourceSpec> algorithms;
    std::atomic<bool>* cancel_flag = nullptr;

    SearchConfig search_cfg;
    MatrixConfig matrix_cfg;
    GraphConfig graph_cfg;
    SortingConfig sorting_cfg;
};

struct CustomRunnerExecutionResult {
    bool success = false;
    bool timed_out = false;
    int exit_code = 0;
    std::string error_message;
    std::string raw_output;
    analysis::BenchmarkRun run;
};

class CustomBenchmarkRunner {
public:
    // Compiles and runs the custom benchmark in an isolated subprocess
    static CustomRunnerExecutionResult execute(const CustomBenchmarkConfig& config);

private:
    static CustomRunnerExecutionResult run_isolated_process(
        const std::string& executable_path,
        const CustomBenchmarkConfig& config,
        const std::string& output_json_file
    );
};

} // namespace custom

#endif // CUSTOM_BENCHMARK_RUNNER_H

