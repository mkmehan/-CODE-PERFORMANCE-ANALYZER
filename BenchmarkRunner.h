#ifndef BENCHMARK_RUNNER_H
#define BENCHMARK_RUNNER_H

#include "BenchmarkConfig.h"
#include "analysis/BenchmarkRun.h"

#include <functional>
#include <string>
#include <vector>

using ProgressCallback = std::function<void(
    int progress_percent,
    const std::string& current_algorithm,
    const std::string& current_input_case,
    size_t current_size,
    int current_iteration,
    int total_iterations,
    double elapsed_seconds
)>;

using CancellationCheck = std::function<bool()>;

class BenchmarkRunner {
private:
    std::vector<Benchmark> benchmarks;
    std::vector<BenchmarkSummary> summaries;
    std::vector<BenchmarkMeasurement> measurements;
    BenchmarkConfig benchmark_config;
    uint64_t overhead = 0;
    analysis::BenchmarkRun last_run_data;
    ProgressCallback progress_callback;
    CancellationCheck cancellation_check;

    void run_experiment(const std::vector<Benchmark>& selected_benchmarks);

public:
    explicit BenchmarkRunner(BenchmarkConfig config = {});

    void add(
        const std::string& key,
        const std::string& name,
        const std::string& theoretical_complexity,
        BenchmarkSetupFunction setup,
        BenchmarkFunction function,
        BenchmarkVerificationFunction verify = {}
    );

    void set_input_sizes(const std::vector<size_t>& sizes);
    void set_input_cases(const std::vector<InputDataCase>& input_cases);
    void set_progress_callback(ProgressCallback cb);
    void set_cancellation_check(CancellationCheck cb);

    const BenchmarkConfig& config() const;
    const std::vector<BenchmarkSummary>& results() const;
    const std::vector<BenchmarkMeasurement>& raw_measurements() const;
    const analysis::BenchmarkRun& last_analysis_run() const;
    analysis::BenchmarkRun to_analysis_run() const;

    void run_all();
    bool run_selected(const std::string& key);
    void run_selected_keys(const std::vector<std::string>& keys);
    void list_benchmarks() const;
};

#endif
