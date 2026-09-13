#ifndef CUSTOM_BENCHMARK_RUNNER_H
#define CUSTOM_BENCHMARK_RUNNER_H

#include "../analysis/BenchmarkRun.h"
#include "CustomBenchmarkCompiler.h"

#include <string>
#include <vector>

namespace custom {

struct CustomBenchmarkConfig {
    std::string dataset_path;
    int target_value = 5000;
    int iterations = 20;
    int warmup_runs = 5;
    int cpu_affinity = -1;
    bool measure_memory = true;
    int timeout_seconds = 15;
    std::vector<AlgorithmSourceSpec> algorithms;
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

