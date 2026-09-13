#ifndef ANALYSIS_TREND_ANALYZER_H
#define ANALYSIS_TREND_ANALYZER_H

#include "BenchmarkRun.h"
#include "ChartData.h"

#include <vector>

namespace analysis {

// Converts a flat vector of BenchmarkRun objects (potentially from history)
// into ChartData suitable for any renderer.
//
// Pipeline inside each method:
//   1. Apply ChartFilter to skip incompatible runs (run-level: input_type, input_file)
//      and incompatible records (record-level: input_distribution, algorithms).
//   2. Accumulate into map: algorithm_key → (input_size → sum, count).
//   3. Average duplicate N values within compatible runs (reduces run-to-run noise).
//   4. Sort series points by x (input_size) ascending.
//   5. Partial series are kept — an algorithm missing data at some N is still included.
//
// The resulting ChartData is completely independent of BenchmarkRun internals.
class TrendAnalyzer {
public:
    // Feature #7: Time vs Input Size
    // y-axis: record.time_mean_ns converted to µs (divide by 1000)
    static ChartData build_time_vs_size(
        const std::vector<BenchmarkRun>& runs,
        const ChartFilter& filter = {}
    );

    // Feature #8: Peak Memory vs Input Size
    // y-axis: record.memory_peak_increase_bytes converted to MB (divide by 1024*1024)
    static ChartData build_memory_vs_size(
        const std::vector<BenchmarkRun>& runs,
        const ChartFilter& filter = {}
    );

    // CLI text-table printers — for verifying chart data before any GUI is built
    static void print_time_table(const ChartData& data);
    static void print_memory_table(const ChartData& data);
};

} // namespace analysis

#endif // ANALYSIS_TREND_ANALYZER_H

