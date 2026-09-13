#include "ComparisonAnalyzer.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>

namespace analysis {

namespace {

std::string format_time_ns(double ns) {
    std::ostringstream out;
    if (ns >= 1e9) {
        out << std::fixed << std::setprecision(2) << (ns / 1e9) << " s";
    } else if (ns >= 1e6) {
        out << std::fixed << std::setprecision(2) << (ns / 1e6) << " ms";
    } else if (ns >= 1e3) {
        out << std::fixed << std::setprecision(2) << (ns / 1e3) << " µs";
    } else {
        out << std::fixed << std::setprecision(0) << ns << " ns";
    }
    return out.str();
}

std::string format_number(size_t n) {
    std::string s = std::to_string(n);
    int insert_pos = static_cast<int>(s.length()) - 3;
    while (insert_pos > 0) {
        s.insert(static_cast<size_t>(insert_pos), ",");
        insert_pos -= 3;
    }
    return s;
}

} // namespace

ComparisonReport ComparisonAnalyzer::compare_records(
    const std::vector<BenchmarkRecord>& records,
    const std::string& input_type,
    const std::string& file_path,
    size_t input_size,
    const std::string& run_id
) {
    ComparisonReport report;
    report.run_id = run_id;

    if (records.empty()) {
        report.error_message = "Cannot compare: no benchmark records provided.";
        return report;
    }

    // Verify condition compatibility: every record must share exact input_type and input_size
    for (const auto& rec : records) {
        if (rec.input_type != input_type || rec.input_size != input_size) {
            report.error_message = "Cannot compare: Custom dataset or input condition differs.";
            return report;
        }
    }

    // Sort by mean time ascending (fastest first)
    auto sorted_records = records;
    std::sort(sorted_records.begin(), sorted_records.end(), [](const BenchmarkRecord& a, const BenchmarkRecord& b) {
        return a.time_mean_ns < b.time_mean_ns;
    });

    const double fastest_time = sorted_records.front().time_mean_ns;
    const double slowest_time = sorted_records.back().time_mean_ns;

    ComparisonGroup group;
    group.input_type = input_type;
    group.file_path = file_path;
    group.input_size = input_size;
    group.fastest_algorithm = sorted_records.front().algorithm_name;
    group.slowest_algorithm = sorted_records.back().algorithm_name;
    group.max_speedup = (fastest_time > 0.0) ? (slowest_time / fastest_time) : 1.0;

    for (size_t i = 0; i < sorted_records.size(); ++i) {
        const auto& r = sorted_records[i];
        ComparisonEntry entry;
        entry.rank = i + 1;
        entry.algorithm_key = r.algorithm;
        entry.algorithm_name = r.algorithm_name;
        entry.time_mean_ns = r.time_mean_ns;
        entry.time_median_ns = r.time_median_ns;

        entry.speedup_vs_slowest = (r.time_mean_ns > 0.0) ? (slowest_time / r.time_mean_ns) : 1.0;
        entry.relative_to_fastest = (fastest_time > 0.0) ? (r.time_mean_ns / fastest_time) : 1.0;
        entry.percentage_faster_than_slowest = (slowest_time > 0.0)
            ? ((slowest_time - r.time_mean_ns) / slowest_time * 100.0)
            : 0.0;

        group.rankings.push_back(std::move(entry));
    }

    report.valid = true;
    report.groups.push_back(std::move(group));
    return report;
}

ComparisonReport ComparisonAnalyzer::compare_run(const BenchmarkRun& run) {
    ComparisonReport report;
    report.run_id = run.run_id;
    report.timestamp = run.timestamp;

    if (run.results.empty()) {
        report.error_message = "Cannot compare: run contains no benchmark results.";
        return report;
    }

    // Group records by (input_type, input_size)
    using ConditionKey = std::pair<std::string, size_t>;
    std::map<ConditionKey, std::vector<BenchmarkRecord>> groups_map;

    for (const auto& rec : run.results) {
        groups_map[{rec.input_type, rec.input_size}].push_back(rec);
    }

    for (const auto& [cond, recs] : groups_map) {
        const std::string file_path = (run.input.type == "custom_file") ? run.input.file_path : "";
        const auto sub_rep = compare_records(recs, cond.first, file_path, cond.second, run.run_id);
        if (sub_rep.valid && !sub_rep.groups.empty()) {
            report.groups.push_back(sub_rep.groups.front());
        }
    }

    report.valid = !report.groups.empty();
    if (!report.valid) {
        report.error_message = "Cannot compare: failed to generate comparison groups.";
    }
    return report;
}

void ComparisonAnalyzer::print_comparison_report(const ComparisonReport& report) {
    if (!report.valid) {
        std::cerr << "\nComparison Error: " << report.error_message << "\n\n";
        return;
    }

    for (size_t g = 0; g < report.groups.size(); ++g) {
        const auto& group = report.groups[g];

        std::cout << "\nALGORITHM COMPARISON\n";
        std::cout << "────────────────────────────────────────────\n\n";
        if (!report.run_id.empty()) {
            std::cout << "Run       : " << report.run_id << '\n';
        }
        if (!group.file_path.empty()) {
            std::cout << "Input     : " << group.file_path << " (" << group.input_type << ")\n";
        } else {
            std::cout << "Input     : " << group.input_type << '\n';
        }
        std::cout << "Elements  : " << format_number(group.input_size) << "\n\n";

        std::cout
            << std::left
            << std::setw(6)  << "Rank"
            << std::setw(18) << "Algorithm"
            << std::setw(14) << "Mean"
            << "Speedup\n"
            << "--------------------------------------------\n";

        for (const auto& entry : group.rankings) {
            std::ostringstream speedup_str;
            speedup_str << std::fixed << std::setprecision(2) << entry.speedup_vs_slowest << "x";

            std::cout
                << std::left
                << std::setw(6)  << entry.rank
                << std::setw(18) << entry.algorithm_name
                << std::setw(14) << format_time_ns(entry.time_mean_ns)
                << speedup_str.str() << '\n';
        }

        std::cout << "--------------------------------------------\n";
        if (group.rankings.size() > 1) {
            std::cout
                << "Fastest   : " << group.fastest_algorithm << " ("
                << std::fixed << std::setprecision(2) << group.max_speedup
                << "x faster than " << group.slowest_algorithm << ")\n";
        }
        std::cout << '\n';
    }
}

} // namespace analysis

