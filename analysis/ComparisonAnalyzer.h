#ifndef ANALYSIS_COMPARISON_ANALYZER_H
#define ANALYSIS_COMPARISON_ANALYZER_H

#include "BenchmarkRun.h"

#include <string>
#include <vector>

namespace analysis {

struct ComparisonEntry {
    size_t rank = 0;
    std::string algorithm_key;
    std::string algorithm_name;
    double time_mean_ns = 0.0;
    double time_median_ns = 0.0;
    double speedup_vs_slowest = 1.0;          // e.g. 2.96x
    double relative_to_fastest = 1.0;         // 1.0x for fastest, >1.0x for slower
    double percentage_faster_than_slowest = 0.0; // ((slowest - current) / slowest) * 100%
};

struct ComparisonGroup {
    std::string input_type;
    std::string file_path;
    size_t input_size = 0;
    std::string fastest_algorithm;
    std::string slowest_algorithm;
    double max_speedup = 1.0;
    std::vector<ComparisonEntry> rankings;
};

struct ComparisonReport {
    bool valid = false;
    std::string error_message;
    std::string run_id;
    std::string timestamp;
    std::vector<ComparisonGroup> groups;
};

class ComparisonAnalyzer {
public:
    // Compare all algorithms within a benchmark run (grouped by identical input conditions)
    static ComparisonReport compare_run(const BenchmarkRun& run);

    // Compare a collection of records ensuring strictly identical input conditions
    static ComparisonReport compare_records(
        const std::vector<BenchmarkRecord>& records,
        const std::string& input_type,
        const std::string& file_path,
        size_t input_size,
        const std::string& run_id = ""
    );

    // Format comparison report to console
    static void print_comparison_report(const ComparisonReport& report);
};

} // namespace analysis

#endif // ANALYSIS_COMPARISON_ANALYZER_H

