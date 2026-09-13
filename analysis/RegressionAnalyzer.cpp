#include "RegressionAnalyzer.h"

#include <cmath>
#include <iomanip>
#include <iostream>
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

std::string format_delta(double delta) {
    std::ostringstream out;
    if (delta > 0) {
        out << "+";
    }
    out << std::fixed << std::setprecision(2) << delta << "%";
    return out.str();
}

} // namespace

bool RegressionAnalyzer::are_runs_compatible(
    const BenchmarkRun& a,
    const BenchmarkRun& b
) {
    // Mode and category must match (e.g. standard sorting vs custom search)
    if (a.benchmark_mode != b.benchmark_mode || a.benchmark_category != b.benchmark_category) {
        return false;
    }

    // If one or both are custom files, the custom dataset should match
    if (a.input.type == "custom_file" && b.input.type == "custom_file") {
        if (!a.input.file_path.empty() && !b.input.file_path.empty() &&
            a.input.file_path != b.input.file_path) {
            return false;
        }
    } else if (a.input.type != b.input.type) {
        return false;
    }

    for (const auto& rec_a : a.results) {
        for (const auto& rec_b : b.results) {
            if (rec_a.algorithm == rec_b.algorithm &&
                rec_a.input_type == rec_b.input_type &&
                rec_a.input_size == rec_b.input_size) {
                return true;
            }
        }
    }
    return false;
}

RegressionReport RegressionAnalyzer::compare_runs(
    const BenchmarkRun& current_run,
    const BenchmarkRun& baseline_run,
    double tolerance_percentage
) {
    RegressionReport report;
    report.current_run_id = current_run.run_id;
    report.baseline_run_id = baseline_run.run_id;
    report.tolerance_percentage = tolerance_percentage;

    if (!are_runs_compatible(current_run, baseline_run)) {
        report.error_message = "Cannot compare: No compatible algorithm/input targets found between baseline and current runs.";
        return report;
    }

    const double eps = 1e-7;

    for (const auto& curr : current_run.results) {
        // Find matching record in baseline
        const BenchmarkRecord* base_match = nullptr;
        for (const auto& base : baseline_run.results) {
            if (base.algorithm == curr.algorithm &&
                base.input_type == curr.input_type &&
                base.input_size == curr.input_size) {
                base_match = &base;
                break;
            }
        }

        if (base_match == nullptr) {
            continue; // No matching target in baseline run
        }

        RegressionRecord rec;
        rec.algorithm_key = curr.algorithm;
        rec.algorithm_name = curr.algorithm_name;
        rec.input_type = curr.input_type;
        rec.input_size = curr.input_size;
        rec.baseline_time_ns = base_match->time_mean_ns;
        rec.current_time_ns = curr.time_mean_ns;
        rec.baseline_memory_bytes = base_match->memory_peak_increase_bytes;
        rec.current_memory_bytes = curr.memory_peak_increase_bytes;

        if (rec.baseline_time_ns > 0.0) {
            rec.delta_percentage = ((rec.current_time_ns - rec.baseline_time_ns) / rec.baseline_time_ns) * 100.0;
        } else {
            rec.delta_percentage = 0.0;
        }

        // Apply tolerance:
        // delta < -tol: Improved (faster)
        // -tol <= delta <= +tol: Stable
        // delta > +tol: Regressed (slower)
        if (rec.delta_percentage < (-tolerance_percentage - eps)) {
            rec.status = RegressionStatus::Improved;
            ++report.improved_count;
        } else if (rec.delta_percentage > (tolerance_percentage + eps)) {
            rec.status = RegressionStatus::Regressed;
            ++report.regressed_count;
        } else {
            rec.status = RegressionStatus::Stable;
            ++report.stable_count;
        }

        if (current_run.system.cpu != baseline_run.system.cpu) {
            rec.notes = "Note: CPU model differed";
        }

        report.records.push_back(std::move(rec));
    }

    report.has_regressions = (report.regressed_count > 0);
    report.valid = !report.records.empty();
    if (!report.valid) {
        report.error_message = "Cannot compare: Zero compatible benchmark records found.";
    }
    return report;
}

void RegressionAnalyzer::print_regression_report(const RegressionReport& report) {
    if (!report.valid) {
        std::cerr << "\nRegression Analysis Error: " << report.error_message << "\n\n";
        return;
    }

    std::cout << "\n================================================================================\n";
    std::cout << "                           RUN-TO-RUN REGRESSION REPORT\n";
    std::cout << "================================================================================\n";
    std::cout << "Current Run   : " << report.current_run_id << '\n';
    std::cout << "Baseline Run  : " << report.baseline_run_id << '\n';
    std::cout << "Tolerance     : ±" << std::fixed << std::setprecision(1) << report.tolerance_percentage << "%\n";
    std::cout << "────────────────────────────────────────────────────────────────────────────────\n";

    std::cout
        << std::left
        << std::setw(16) << "Algorithm"
        << std::setw(12) << "Input"
        << std::setw(8)  << "N"
        << std::setw(14) << "Baseline"
        << std::setw(14) << "Current"
        << std::setw(12) << "Delta %"
        << "Status\n"
        << "────────────────────────────────────────────────────────────────────────────────\n";

    for (const auto& rec : report.records) {
        std::string status_str;
        switch (rec.status) {
            case RegressionStatus::Improved:
                status_str = "IMPROVED ✓";
                break;
            case RegressionStatus::Stable:
                status_str = "STABLE";
                break;
            case RegressionStatus::Regressed:
                status_str = "REGRESSION ✗";
                break;
        }

        std::cout
            << std::left
            << std::setw(16) << rec.algorithm_name
            << std::setw(12) << rec.input_type
            << std::setw(8)  << rec.input_size
            << std::setw(14) << format_time_ns(rec.baseline_time_ns)
            << std::setw(14) << format_time_ns(rec.current_time_ns)
            << std::setw(12) << format_delta(rec.delta_percentage)
            << status_str << '\n';
    }

    std::cout << "────────────────────────────────────────────────────────────────────────────────\n";
    std::cout << "Summary: " << report.improved_count << " Improved, "
              << report.stable_count << " Stable, "
              << report.regressed_count << " Regressed.\n";

    if (report.has_regressions) {
        std::cout << "OVERALL: ⚠ PERFORMANCE REGRESSION DETECTED\n";
    } else {
        std::cout << "OVERALL: ✓ NO REGRESSIONS DETECTED (Within ±"
                  << std::fixed << std::setprecision(1) << report.tolerance_percentage << "% tolerance)\n";
    }
    std::cout << "================================================================================\n\n";
}

} // namespace analysis

