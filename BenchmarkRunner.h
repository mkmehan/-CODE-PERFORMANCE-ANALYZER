#ifndef BENCHMARK_RUNNER_H
#define BENCHMARK_RUNNER_H

#include "BenchmarkConfig.h"
#include "analysis/BenchmarkRun.h"

#include <string>
#include <vector>

class BenchmarkRunner {
private:
    std::vector<Benchmark> benchmarks;
    std::vector<BenchmarkSummary> summaries;
    std::vector<BenchmarkMeasurement> measurements;
    BenchmarkConfig benchmark_config;
    uint64_t overhead = 0;
    analysis::BenchmarkRun last_run_data;

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

    const BenchmarkConfig& config() const;
    const std::vector<BenchmarkSummary>& results() const;
    const std::vector<BenchmarkMeasurement>& raw_measurements() const;
    const analysis::BenchmarkRun& last_analysis_run() const;
    analysis::BenchmarkRun to_analysis_run() const;

    void run_all();
    bool run_selected(const std::string& key);
    void list_benchmarks() const;
};

#endif
