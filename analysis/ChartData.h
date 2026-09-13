#ifndef ANALYSIS_CHART_DATA_H
#define ANALYSIS_CHART_DATA_H

#include <string>
#include <vector>

namespace analysis {

// A single (x, y) data point.
// x = input_size (N), y = time in µs or memory in MB
struct ChartPoint {
    double x = 0.0;
    double y = 0.0;
};

// One algorithm's complete data series across all input sizes
struct AlgorithmSeries {
    std::string algorithm_key;   // e.g. "quicksort"
    std::string algorithm_name;  // e.g. "Quick Sort"
    std::vector<ChartPoint> points; // sorted by x ascending; may be partial
};

// Controls which runs and records are included in a chart.
// All fields are optional (empty = no filter applied for that field).
struct ChartFilter {
    // Run-level filters
    std::string input_type;           // "generated" or "custom_file"
    std::string input_file;           // custom file path — empty = all files

    // Record-level filters
    std::string input_distribution;   // "Random", "Sorted", "Reverse Sorted", etc.
    std::vector<std::string> algorithms; // algorithm keys — empty = all
};

// Output structure consumed by any renderer (CLI table, Qt widget, HTML, etc.)
// The renderer never needs to look at BenchmarkRun, BenchmarkRecord, or JSON.
struct ChartData {
    std::string title;
    std::string x_label = "Input Size (N)";
    std::string y_label;
    ChartFilter filter_used;           // Preserved for display / debugging
    std::vector<AlgorithmSeries> series;
    bool valid = false;
    std::string error_message;
};

} // namespace analysis

#endif // ANALYSIS_CHART_DATA_H

