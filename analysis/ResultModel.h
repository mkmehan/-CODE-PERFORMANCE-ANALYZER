#ifndef ANALYSIS_RESULT_MODEL_H
#define ANALYSIS_RESULT_MODEL_H

#include <string>
#include <cstddef>
#include <cstdint>

namespace analysis {

struct BenchmarkRecord {
    std::string algorithm;          // Key, e.g. "quicksort"
    std::string algorithm_name;     // Display name, e.g. "Quick Sort"
    std::string input_type;         // e.g. "Random", "Custom File"
    size_t input_size = 0;
    bool verified = false;

    // Time statistics (nanoseconds)
    double time_min_ns = 0.0;
    double time_max_ns = 0.0;
    double time_mean_ns = 0.0;
    double time_median_ns = 0.0;
    double time_stddev_ns = 0.0;

    // CPU Cycle statistics
    double cycles_min = 0.0;
    double cycles_max = 0.0;
    double cycles_mean = 0.0;
    double cycles_median = 0.0;
    double cycles_stddev = 0.0;

    // Memory statistics
    uint64_t memory_peak_increase_bytes = 0;
    int64_t memory_net_change_bytes = 0; // Signed to accurately handle memory deallocations / negative net changes
};

// Safe alias inside namespace analysis
using BenchmarkResult = BenchmarkRecord;

} // namespace analysis

#endif // ANALYSIS_RESULT_MODEL_H

