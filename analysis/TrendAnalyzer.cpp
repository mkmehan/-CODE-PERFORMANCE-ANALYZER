#include "TrendAnalyzer.h"

#include <algorithm>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace analysis {

namespace {

// ─────────────────────────────────────────────────────────────────────────────
// Internal accumulation structure
// ─────────────────────────────────────────────────────────────────────────────

struct PointAccum {
    std::string algorithm_name;
    double sum   = 0.0;
    size_t count = 0;
};

// algorithm_key → (input_size → PointAccum)
using AccumMap = std::map<std::string, std::map<size_t, PointAccum>>;

// ─────────────────────────────────────────────────────────────────────────────
// Filter helpers
// ─────────────────────────────────────────────────────────────────────────────

bool run_passes_filter(const BenchmarkRun& run, const ChartFilter& f) {
    if (!f.input_type.empty() && run.input.type != f.input_type) return false;
    if (!f.input_file.empty() && run.input.file_path != f.input_file) return false;
    return true;
}

bool record_passes_filter(const BenchmarkRecord& rec, const ChartFilter& f) {
    if (!f.input_distribution.empty() && rec.input_type != f.input_distribution) return false;
    if (!f.algorithms.empty()) {
        bool found = false;
        for (const auto& alg : f.algorithms) {
            if (alg == rec.algorithm) { found = true; break; }
        }
        if (!found) return false;
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// N header formatting helpers
// ─────────────────────────────────────────────────────────────────────────────

std::string format_n_header(double n) {
    auto ni = static_cast<size_t>(n);
    if (ni < 1000) {
        return "N=" + std::to_string(ni);
    }
    if (ni < 1000000) {
        size_t k = ni / 1000;
        size_t r = (ni % 1000) / 100;
        if (r == 0) return "N=" + std::to_string(k) + "K";
        return "N=" + std::to_string(k) + "." + std::to_string(r) + "K";
    }
    return "N=" + std::to_string(ni / 1000000) + "M";
}

// ─────────────────────────────────────────────────────────────────────────────
// Value formatting helpers
// ─────────────────────────────────────────────────────────────────────────────

std::string format_time_us(double us) {
    std::ostringstream oss;
    if (us < 1.0) {
        oss << std::fixed << std::setprecision(3) << us;
    } else if (us < 10.0) {
        oss << std::fixed << std::setprecision(2) << us;
    } else if (us < 100.0) {
        oss << std::fixed << std::setprecision(1) << us;
    } else if (us < 10000.0) {
        oss << std::fixed << std::setprecision(0) << us;
    } else {
        // Show in ms for readability
        oss << std::fixed << std::setprecision(1) << (us / 1000.0) << " ms";
        return oss.str();
    }
    oss << " µs";
    return oss.str();
}

std::string format_memory_mb(double mb) {
    std::ostringstream oss;
    if (mb < 0.001) {
        oss << std::fixed << std::setprecision(4) << mb;
    } else if (mb < 0.1) {
        oss << std::fixed << std::setprecision(3) << mb;
    } else if (mb < 10.0) {
        oss << std::fixed << std::setprecision(2) << mb;
    } else {
        oss << std::fixed << std::setprecision(1) << mb;
    }
    oss << " MB";
    return oss.str();
}

// ─────────────────────────────────────────────────────────────────────────────
// Convert AccumMap → ChartData (shared by time and memory builders)
// ─────────────────────────────────────────────────────────────────────────────

ChartData build_from_accum(
    const AccumMap& accum,
    const ChartFilter& filter,
    const std::string& title,
    const std::string& y_label
) {
    ChartData data;
    data.title       = title;
    data.x_label     = "Input Size (N)";
    data.y_label     = y_label;
    data.filter_used = filter;

    if (accum.empty()) {
        data.valid         = false;
        data.error_message = "No compatible benchmark records found in history.";
        return data;
    }

    for (const auto& [alg_key, size_map] : accum) {
        AlgorithmSeries series;
        series.algorithm_key = alg_key;

        for (const auto& [n, pt] : size_map) {
            if (pt.count == 0) continue;
            series.algorithm_name = pt.algorithm_name;
            ChartPoint cp;
            cp.x = static_cast<double>(n);
            cp.y = pt.sum / static_cast<double>(pt.count); // average
            series.points.push_back(cp);
        }

        // Points are already x-sorted because std::map keys are sorted,
        // but sort explicitly in case the interface changes later.
        std::sort(series.points.begin(), series.points.end(),
                  [](const ChartPoint& a, const ChartPoint& b) { return a.x < b.x; });

        if (!series.points.empty()) {
            data.series.push_back(std::move(series));
        }
    }

    data.valid = !data.series.empty();
    if (!data.valid) {
        data.error_message = "No data points produced after applying filter.";
    }
    return data;
}

// ─────────────────────────────────────────────────────────────────────────────
// Generic table printer (used by print_time_table and print_memory_table)
// ─────────────────────────────────────────────────────────────────────────────

void print_trend_table(
    const ChartData& data,
    const std::string& header_title,
    const std::function<std::string(double)>& format_y
) {
    if (!data.valid) {
        std::cerr << "\nChart Data Error: " << data.error_message << "\n\n";
        return;
    }

    // Collect all unique N values across all series
    std::vector<double> all_n;
    for (const auto& s : data.series) {
        for (const auto& pt : s.points) {
            bool found = false;
            for (double n : all_n) { if (n == pt.x) { found = true; break; } }
            if (!found) all_n.push_back(pt.x);
        }
    }
    std::sort(all_n.begin(), all_n.end());

    // Build filter description for display
    std::string filter_info;
    if (!data.filter_used.input_type.empty()) {
        filter_info = data.filter_used.input_type;
    }
    if (!data.filter_used.input_distribution.empty()) {
        if (!filter_info.empty()) filter_info += " / ";
        filter_info += data.filter_used.input_distribution;
    }
    if (!data.filter_used.input_file.empty()) {
        if (!filter_info.empty()) filter_info += " — ";
        filter_info += data.filter_used.input_file;
    }

    // Column widths
    const int col_alg  = 18;
    const int col_n    = 13;
    const int total_w  = col_alg + col_n * static_cast<int>(all_n.size());
    const std::string sep(total_w, '-');

    std::cout << "\n" << header_title << "\n";
    std::cout << sep << "\n";
    if (!filter_info.empty()) {
        std::cout << "Input: " << filter_info << "\n\n";
    }

    // Header row
    std::cout << std::left << std::setw(col_alg) << "Algorithm";
    for (double n : all_n) {
        std::cout << std::right << std::setw(col_n) << format_n_header(n);
    }
    std::cout << "\n" << sep << "\n";

    // Data rows
    for (const auto& series : data.series) {
        const std::string& display =
            series.algorithm_name.empty() ? series.algorithm_key : series.algorithm_name;
        std::cout << std::left << std::setw(col_alg) << display;

        // Build lookup map for this series
        std::map<double, double> point_map;
        for (const auto& pt : series.points) point_map[pt.x] = pt.y;

        for (double n : all_n) {
            auto it = point_map.find(n);
            // "—" for missing points (partial series)
            const std::string cell = (it != point_map.end()) ? format_y(it->second) : "\xe2\x80\x94";
            std::cout << std::right << std::setw(col_n) << cell;
        }
        std::cout << "\n";
    }

    std::cout << sep << "\n";
    std::cout << "Y-axis : " << data.y_label << "\n";
    if (all_n.size() > 1) {
        std::cout << "Series : " << data.series.size() << " algorithm(s)  |  "
                  << all_n.size() << " input size(s)\n";
    }
    std::cout << "\n";
}

} // namespace (anonymous)

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

ChartData TrendAnalyzer::build_time_vs_size(
    const std::vector<BenchmarkRun>& runs,
    const ChartFilter& filter
) {
    AccumMap accum;

    for (const auto& run : runs) {
        if (!run_passes_filter(run, filter)) continue;
        for (const auto& rec : run.results) {
            if (!record_passes_filter(rec, filter)) continue;
            auto& pt = accum[rec.algorithm][rec.input_size];
            pt.algorithm_name = rec.algorithm_name;
            pt.sum  += rec.time_mean_ns / 1000.0; // ns → µs
            pt.count++;
        }
    }

    return build_from_accum(accum, filter, "TIME TREND", "Mean Time (\xc2\xb5s)");
}

ChartData TrendAnalyzer::build_memory_vs_size(
    const std::vector<BenchmarkRun>& runs,
    const ChartFilter& filter
) {
    AccumMap accum;

    for (const auto& run : runs) {
        if (!run_passes_filter(run, filter)) continue;
        for (const auto& rec : run.results) {
            if (!record_passes_filter(rec, filter)) continue;
            auto& pt = accum[rec.algorithm][rec.input_size];
            pt.algorithm_name = rec.algorithm_name;
            pt.sum  += static_cast<double>(rec.memory_peak_increase_bytes)
                       / (1024.0 * 1024.0); // bytes → MB
            pt.count++;
        }
    }

    return build_from_accum(accum, filter, "MEMORY TREND", "Peak Memory Increase (MB)");
}

void TrendAnalyzer::print_time_table(const ChartData& data) {
    print_trend_table(data, "TIME TREND", format_time_us);
}

void TrendAnalyzer::print_memory_table(const ChartData& data) {
    print_trend_table(data, "MEMORY TREND", format_memory_mb);
}

} // namespace analysis

