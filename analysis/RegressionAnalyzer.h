#ifndef ANALYSIS_REGRESSION_ANALYZER_H
#define ANALYSIS_REGRESSION_ANALYZER_H

#include "BenchmarkRun.h"

#include <string>
#include <vector>

namespace analysis {

enum class RegressionStatus {
    Improved,
    Stable,
    Regressed
};

struct RegressionRecord {
    std::string algorithm_key;
    std::string algorithm_name;
    std::string input_type;
    size_t input_size = 0;
    double baseline_time_ns = 0.0;
    double current_time_ns = 0.0;
    double delta_percentage = 0.0;
    uint64_t baseline_memory_bytes = 0;
    uint64_t current_memory_bytes = 0;
    RegressionStatus status = RegressionStatus::Stable;
    std::string notes;
};

struct RegressionReport {
    bool valid = false;
    std::string error_message;
    std::string baseline_run_id;
    std::string current_run_id;
    double tolerance_percentage = 5.0;
    std::vector<RegressionRecord> records;
    size_t improved_count = 0;
    size_t stable_count = 0;
    size_t regressed_count = 0;
    bool has_regressions = false;
};

class RegressionAnalyzer {
public:
    // Compare a current run against a baseline run using configurable tolerance threshold (default ±5.0%)
    static RegressionReport compare_runs(
        const BenchmarkRun& current_run,
        const BenchmarkRun& baseline_run,
        double tolerance_percentage = 5.0
    );

    // Check if two runs share at least one compatible benchmark target
    static bool are_runs_compatible(
        const BenchmarkRun& a,
        const BenchmarkRun& b
    );

    // Formatted terminal presentation
    static void print_regression_report(const RegressionReport& report);
};

} // namespace analysis

#endif // ANALYSIS_REGRESSION_ANALYZER_H

