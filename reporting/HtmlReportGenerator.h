#ifndef REPORTING_HTML_REPORT_GENERATOR_H
#define REPORTING_HTML_REPORT_GENERATOR_H

#include "../analysis/BenchmarkRun.h"
#include "../analysis/ChartData.h"
#include "../analysis/ComparisonAnalyzer.h"
#include "../analysis/RegressionAnalyzer.h"

#include <string>
#include <vector>

namespace reporting {

struct ReportOptions {
    std::string output_directory = "results/reports";
    bool auto_open_in_browser = false;
    bool include_offline_assets = true;
};

class HtmlReportGenerator {
public:
    // Generate complete HTML report with pre-computed analysis models
    static std::string generate_report(
        const analysis::BenchmarkRun& run,
        const analysis::ComparisonReport& comparison,
        const analysis::RegressionReport& regression,
        const analysis::ChartData& time_chart,
        const analysis::ChartData& memory_chart,
        const ReportOptions& options = {}
    );

    // Convenience generator that automatically computes comparison, regression,
    // and trend charts from the current run and historical runs
    static std::string generate_from_run(
        const analysis::BenchmarkRun& run,
        const std::vector<analysis::BenchmarkRun>& historical_runs = {},
        const ReportOptions& options = {}
    );

    // Open an HTML file in the user's default web browser
    static bool open_in_browser(const std::string& html_filepath);

private:
    // Helper to ensure local assets directory exists with chart.min.js
    static void ensure_assets_installed(const std::string& report_dir);

    // HTML escape utility
    static std::string escape_html(const std::string& data);

    // Formatter helpers
    static std::string format_time(double ns);
    static std::string format_memory(uint64_t bytes);
};

} // namespace reporting

#endif // REPORTING_HTML_REPORT_GENERATOR_H

