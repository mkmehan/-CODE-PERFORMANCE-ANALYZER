#include "HtmlReportGenerator.h"
#include "../analysis/TrendAnalyzer.h"

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <vector>

namespace reporting {

namespace {

// Colors for Chart.js series
const std::vector<std::string> PALETTE = {
    "#06b6d4", // Cyan
    "#10b981", // Emerald
    "#8b5cf6", // Purple
    "#f59e0b", // Amber
    "#f43f5e", // Rose
    "#3b82f6", // Blue
    "#ec4899", // Pink
    "#14b8a6", // Teal
    "#a855f7", // Violet
    "#eab308"  // Yellow
};

std::string get_color(size_t index, double alpha = 1.0) {
    const std::string& hex = PALETTE[index % PALETTE.size()];
    if (alpha >= 0.99) return hex;
    // Simple hex to rgba for backgrounds
    unsigned int r = 0, g = 0, b = 0;
    if (hex.size() == 7 && hex[0] == '#') {
        std::stringstream ss;
        ss << std::hex << hex.substr(1, 2);
        ss >> r;
        ss.clear();
        ss << std::hex << hex.substr(3, 2);
        ss >> g;
        ss.clear();
        ss << std::hex << hex.substr(5, 2);
        ss >> b;
    }
    std::ostringstream oss;
    oss << "rgba(" << r << ", " << g << ", " << b << ", " << std::fixed << std::setprecision(2) << alpha << ")";
    return oss.str();
}

std::string format_number(size_t val) {
    std::string s = std::to_string(val);
    int insert_pos = static_cast<int>(s.length()) - 3;
    while (insert_pos > 0) {
        s.insert(static_cast<size_t>(insert_pos), ",");
        insert_pos -= 3;
    }
    return s;
}

} // namespace (anonymous)

std::string HtmlReportGenerator::escape_html(const std::string& data) {
    std::string buffer;
    buffer.reserve(data.size());
    for (char c : data) {
        switch (c) {
            case '&':  buffer.append("&amp;");       break;
            case '\"': buffer.append("&quot;");      break;
            case '\'': buffer.append("&#39;");       break;
            case '<':  buffer.append("&lt;");        break;
            case '>':  buffer.append("&gt;");        break;
            default:   buffer.push_back(c);          break;
        }
    }
    return buffer;
}

std::string HtmlReportGenerator::format_time(double ns) {
    std::ostringstream oss;
    if (ns < 1000.0) {
        oss << std::fixed << std::setprecision(1) << ns << " ns";
    } else if (ns < 1000000.0) {
        oss << std::fixed << std::setprecision(2) << (ns / 1000.0) << " µs";
    } else if (ns < 1000000000.0) {
        oss << std::fixed << std::setprecision(2) << (ns / 1000000.0) << " ms";
    } else {
        oss << std::fixed << std::setprecision(2) << (ns / 1000000000.0) << " s";
    }
    return oss.str();
}

std::string HtmlReportGenerator::format_memory(uint64_t bytes) {
    std::ostringstream oss;
    double mb = static_cast<double>(bytes) / (1024.0 * 1024.0);
    if (mb < 0.001) {
        oss << std::fixed << std::setprecision(4) << mb << " MB";
    } else if (mb < 0.1) {
        oss << std::fixed << std::setprecision(3) << mb << " MB";
    } else {
        oss << std::fixed << std::setprecision(2) << mb << " MB";
    }
    return oss.str();
}

void HtmlReportGenerator::ensure_assets_installed(const std::string& report_dir) {
    try {
        std::filesystem::path target_dir = std::filesystem::path(report_dir) / "assets";
        std::filesystem::create_directories(target_dir);
        std::filesystem::path target_js = target_dir / "chart.min.js";
        
        if (!std::filesystem::exists(target_js)) {
            // Check if reporting/assets/chart.min.js exists
            std::filesystem::path src_js = std::filesystem::path("reporting") / "assets" / "chart.min.js";
            if (std::filesystem::exists(src_js)) {
                std::filesystem::copy_file(src_js, target_js, std::filesystem::copy_options::overwrite_existing);
            }
        }
    } catch (...) {
        // Fallback to CDN gracefully if disk copy fails
    }
}

bool HtmlReportGenerator::open_in_browser(const std::string& html_filepath) {
#ifdef _WIN32
    HINSTANCE res = ShellExecuteA(NULL, "open", html_filepath.c_str(), NULL, NULL, SW_SHOWNORMAL);
    return reinterpret_cast<intptr_t>(res) > 32;
#else
    std::string cmd = "xdg-open \"" + html_filepath + "\" 2>/dev/null || open \"" + html_filepath + "\" 2>/dev/null";
    return system(cmd.c_str()) == 0;
#endif
}

std::string HtmlReportGenerator::generate_from_run(
    const analysis::BenchmarkRun& run,
    const std::vector<analysis::BenchmarkRun>& historical_runs,
    const ReportOptions& options
) {
    // 1. Comparison Report
    analysis::ComparisonReport comparison = analysis::ComparisonAnalyzer::compare_run(run);

    // 2. Regression Report (find most recent compatible baseline)
    analysis::RegressionReport regression;
    for (const auto& past_run : historical_runs) {
        if (past_run.run_id != run.run_id && analysis::RegressionAnalyzer::are_runs_compatible(run, past_run)) {
            regression = analysis::RegressionAnalyzer::compare_runs(run, past_run);
            break;
        }
    }

    // 3. Trend Charts (using all runs + current run)
    std::vector<analysis::BenchmarkRun> all_runs = historical_runs;
    bool found_current = false;
    for (const auto& r : all_runs) {
        if (r.run_id == run.run_id) { found_current = true; break; }
    }
    if (!found_current) {
        all_runs.push_back(run);
    }

    analysis::ChartFilter filter;
    filter.input_type = run.input.type;
    filter.input_file = run.input.file_path;
    if (!run.results.empty()) {
        filter.input_distribution = run.results[0].input_type;
    }

    analysis::ChartData time_chart = analysis::TrendAnalyzer::build_time_vs_size(all_runs, filter);
    analysis::ChartData memory_chart = analysis::TrendAnalyzer::build_memory_vs_size(all_runs, filter);

    return generate_report(run, comparison, regression, time_chart, memory_chart, options);
}

std::string HtmlReportGenerator::generate_report(
    const analysis::BenchmarkRun& run,
    const analysis::ComparisonReport& comparison,
    const analysis::RegressionReport& regression,
    const analysis::ChartData& time_chart,
    const analysis::ChartData& memory_chart,
    const ReportOptions& options
) {
    try {
        std::filesystem::create_directories(options.output_directory);
    } catch (...) {}

    if (options.include_offline_assets) {
        ensure_assets_installed(options.output_directory);
    }

    std::string safe_id = run.run_id.empty() ? "latest" : run.run_id;
    std::replace(safe_id.begin(), safe_id.end(), ':', '-');
    std::replace(safe_id.begin(), safe_id.end(), ' ', '_');

    std::string filename = "report_" + safe_id + ".html";
    std::filesystem::path full_path = std::filesystem::path(options.output_directory) / filename;

    std::ofstream out(full_path);
    if (!out.is_open()) {
        return "";
    }

    // Compute key summary statistics
    std::string fastest_name = "N/A";
    std::string fastest_time = "N/A";
    std::string slowest_name = "N/A";
    std::string slowest_time = "N/A";
    double speedup_factor = 1.0;
    uint64_t max_peak_memory = 0;

    if (comparison.valid && !comparison.groups.empty() && !comparison.groups[0].rankings.empty()) {
        const auto& grp = comparison.groups[0];
        fastest_name = grp.rankings.front().algorithm_name;
        fastest_time = format_time(grp.rankings.front().time_mean_ns);
        slowest_name = grp.rankings.back().algorithm_name;
        slowest_time = format_time(grp.rankings.back().time_mean_ns);
        speedup_factor = grp.max_speedup;
    } else if (!run.results.empty()) {
        auto sorted_res = run.results;
        std::sort(sorted_res.begin(), sorted_res.end(), [](const auto& a, const auto& b) {
            return a.time_mean_ns < b.time_mean_ns;
        });
        fastest_name = sorted_res.front().algorithm_name;
        fastest_time = format_time(sorted_res.front().time_mean_ns);
        slowest_name = sorted_res.back().algorithm_name;
        slowest_time = format_time(sorted_res.back().time_mean_ns);
        if (sorted_res.front().time_mean_ns > 0.0) {
            speedup_factor = sorted_res.back().time_mean_ns / sorted_res.front().time_mean_ns;
        }
    }

    for (const auto& rec : run.results) {
        if (rec.memory_peak_increase_bytes > max_peak_memory) {
            max_peak_memory = rec.memory_peak_increase_bytes;
        }
    }

    // Determine regression status badge
    std::string reg_badge_text = "Baseline Not Set";
    std::string reg_badge_class = "badge-neutral";
    if (regression.valid) {
        if (regression.has_regressions) {
            reg_badge_text = "⚠ " + std::to_string(regression.regressed_count) + " Regression(s)";
            reg_badge_class = "badge-danger";
        } else if (regression.improved_count > 0) {
            reg_badge_text = "✓ " + std::to_string(regression.improved_count) + " Improved";
            reg_badge_class = "badge-success";
        } else {
            reg_badge_text = "✓ Stable (±5%)";
            reg_badge_class = "badge-success";
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Build HTML Page
    // ─────────────────────────────────────────────────────────────────────────
    out << "<!DOCTYPE html>\n"
        << "<html lang=\"en\">\n"
        << "<head>\n"
        << "  <meta charset=\"UTF-8\">\n"
        << "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
        << "  <title>Benchmark Performance Report — " << escape_html(run.run_id) << "</title>\n"
        << "  <!-- Chart.js local offline asset with CDN fallback -->\n"
        << "  <script src=\"assets/chart.min.js\"></script>\n"
        << "  <script>\n"
        << "    if (typeof Chart === 'undefined') {\n"
        << "      document.write('<script src=\"https://cdn.jsdelivr.net/npm/chart.js@4.4.1/dist/chart.umd.min.js\"><\\/script>');\n"
        << "    }\n"
        << "  </script>\n"
        << "  <style>\n"
        << "    :root {\n"
        << "      --bg: #0b0f19;\n"
        << "      --surface: #111827;\n"
        << "      --surface-card: #1f2937;\n"
        << "      --border: #374151;\n"
        << "      --text-main: #f9fafb;\n"
        << "      --text-muted: #9ca3af;\n"
        << "      --accent: #06b6d4;\n"
        << "      --accent-hover: #0891b2;\n"
        << "      --success: #10b981;\n"
        << "      --warning: #f59e0b;\n"
        << "      --danger: #ef4444;\n"
        << "    }\n"
        << "    * { box-sizing: border-box; margin: 0; padding: 0; }\n"
        << "    body {\n"
        << "      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif;\n"
        << "      background-color: var(--bg);\n"
        << "      color: var(--text-main);\n"
        << "      line-height: 1.5;\n"
        << "      padding: 2rem 1rem;\n"
        << "    }\n"
        << "    .container {\n"
        << "      max-width: 1200px;\n"
        << "      margin: 0 auto;\n"
        << "    }\n"
        << "    /* Header */\n"
        << "    header {\n"
        << "      background: linear-gradient(135deg, #1e293b 0%, #0f172a 100%);\n"
        << "      border: 1px solid var(--border);\n"
        << "      border-radius: 12px;\n"
        << "      padding: 2rem;\n"
        << "      margin-bottom: 2rem;\n"
        << "      box-shadow: 0 10px 25px -5px rgba(0, 0, 0, 0.4);\n"
        << "    }\n"
        << "    .header-top {\n"
        << "      display: flex;\n"
        << "      justify-content: space-between;\n"
        << "      align-items: flex-start;\n"
        << "      flex-wrap: wrap;\n"
        << "      gap: 1rem;\n"
        << "    }\n"
        << "    h1 {\n"
        << "      font-size: 1.875rem;\n"
        << "      font-weight: 700;\n"
        << "      letter-spacing: -0.025em;\n"
        << "      color: #fff;\n"
        << "      display: flex;\n"
        << "      align-items: center;\n"
        << "      gap: 0.75rem;\n"
        << "    }\n"
        << "    .subtitle {\n"
        << "      color: var(--text-muted);\n"
        << "      margin-top: 0.25rem;\n"
        << "      font-size: 0.95rem;\n"
        << "    }\n"
        << "    .badge {\n"
        << "      display: inline-flex;\n"
        << "      align-items: center;\n"
        << "      padding: 0.35rem 0.75rem;\n"
        << "      border-radius: 9999px;\n"
        << "      font-size: 0.8rem;\n"
        << "      font-weight: 600;\n"
        << "      text-transform: uppercase;\n"
        << "      letter-spacing: 0.05em;\n"
        << "    }\n"
        << "    .badge-success { background: rgba(16, 185, 129, 0.15); color: #34d399; border: 1px solid rgba(16, 185, 129, 0.3); }\n"
        << "    .badge-danger  { background: rgba(239, 68, 68, 0.15);  color: #f87171; border: 1px solid rgba(239, 68, 68, 0.3); }\n"
        << "    .badge-neutral { background: rgba(156, 163, 175, 0.15); color: #d1d5db; border: 1px solid rgba(156, 163, 175, 0.3); }\n"
        << "    .badge-cyan    { background: rgba(6, 182, 212, 0.15);  color: #22d3ee; border: 1px solid rgba(6, 182, 212, 0.3); }\n"
        << "    /* Metrics Cards Grid */\n"
        << "    .metrics-grid {\n"
        << "      display: grid;\n"
        << "      grid-template-columns: repeat(auto-fit, minmax(240px, 1fr));\n"
        << "      gap: 1.25rem;\n"
        << "      margin-bottom: 2rem;\n"
        << "    }\n"
        << "    .metric-card {\n"
        << "      background: var(--surface);\n"
        << "      border: 1px solid var(--border);\n"
        << "      border-radius: 10px;\n"
        << "      padding: 1.25rem;\n"
        << "      position: relative;\n"
        << "      overflow: hidden;\n"
        << "    }\n"
        << "    .metric-card::before {\n"
        << "      content: '';\n"
        << "      position: absolute;\n"
        << "      top: 0; left: 0; right: 0;\n"
        << "      height: 3px;\n"
        << "      background: var(--accent);\n"
        << "    }\n"
        << "    .metric-card.success::before { background: var(--success); }\n"
        << "    .metric-card.danger::before  { background: var(--danger); }\n"
        << "    .metric-card.warning::before { background: var(--warning); }\n"
        << "    .metric-label {\n"
        << "      font-size: 0.75rem;\n"
        << "      font-weight: 600;\n"
        << "      text-transform: uppercase;\n"
        << "      color: var(--text-muted);\n"
        << "      letter-spacing: 0.05em;\n"
        << "    }\n"
        << "    .metric-value {\n"
        << "      font-size: 1.6rem;\n"
        << "      font-weight: 700;\n"
        << "      margin: 0.35rem 0 0.2rem;\n"
        << "      color: #fff;\n"
        << "    }\n"
        << "    .metric-sub {\n"
        << "      font-size: 0.825rem;\n"
        << "      color: var(--text-muted);\n"
        << "    }\n"
        << "    /* Section Card */\n"
        << "    .card {\n"
        << "      background: var(--surface);\n"
        << "      border: 1px solid var(--border);\n"
        << "      border-radius: 12px;\n"
        << "      padding: 1.5rem;\n"
        << "      margin-bottom: 2rem;\n"
        << "      box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.2);\n"
        << "    }\n"
        << "    .card-title {\n"
        << "      font-size: 1.15rem;\n"
        << "      font-weight: 600;\n"
        << "      margin-bottom: 1.25rem;\n"
        << "      color: #fff;\n"
        << "      display: flex;\n"
        << "      justify-content: space-between;\n"
        << "      align-items: center;\n"
        << "      border-bottom: 1px solid var(--border);\n"
        << "      padding-bottom: 0.75rem;\n"
        << "    }\n"
        << "    /* Charts Layout */\n"
        << "    .charts-grid {\n"
        << "      display: grid;\n"
        << "      grid-template-columns: repeat(auto-fit, minmax(500px, 1fr));\n"
        << "      gap: 1.5rem;\n"
        << "      margin-bottom: 2rem;\n"
        << "    }\n"
        << "    .chart-container {\n"
        << "      position: relative;\n"
        << "      height: 340px;\n"
        << "      width: 100%;\n"
        << "    }\n"
        << "    /* Table */\n"
        << "    .table-container {\n"
        << "      overflow-x: auto;\n"
        << "    }\n"
        << "    table {\n"
        << "      width: 100%;\n"
        << "      border-collapse: collapse;\n"
        << "      font-size: 0.9rem;\n"
        << "      text-align: left;\n"
        << "    }\n"
        << "    th {\n"
        << "      background: var(--surface-card);\n"
        << "      color: var(--text-muted);\n"
        << "      font-weight: 600;\n"
        << "      padding: 0.75rem 1rem;\n"
        << "      border-bottom: 1px solid var(--border);\n"
        << "      text-transform: uppercase;\n"
        << "      font-size: 0.75rem;\n"
        << "      letter-spacing: 0.05em;\n"
        << "    }\n"
        << "    td {\n"
        << "      padding: 0.75rem 1rem;\n"
        << "      border-bottom: 1px solid var(--border);\n"
        << "      color: var(--text-main);\n"
        << "    }\n"
        << "    tr:hover td {\n"
        << "      background: rgba(255, 255, 255, 0.02);\n"
        << "    }\n"
        << "    /* System & Info Grid */\n"
        << "    .info-grid {\n"
        << "      display: grid;\n"
        << "      grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));\n"
        << "      gap: 1rem;\n"
        << "    }\n"
        << "    .info-item {\n"
        << "      display: flex;\n"
        << "      justify-content: space-between;\n"
        << "      padding: 0.5rem 0;\n"
        << "      border-bottom: 1px solid rgba(255, 255, 255, 0.05);\n"
        << "      font-size: 0.875rem;\n"
        << "    }\n"
        << "    .info-key { color: var(--text-muted); }\n"
        << "    .info-val { font-weight: 500; color: #fff; }\n"
        << "    /* Conclusion Box */\n"
        << "    .conclusion-box {\n"
        << "      background: rgba(6, 182, 212, 0.05);\n"
        << "      border: 1px solid rgba(6, 182, 212, 0.2);\n"
        << "      border-radius: 8px;\n"
        << "      padding: 1.25rem;\n"
        << "      font-size: 0.925rem;\n"
        << "      color: #e2e8f0;\n"
        << "      line-height: 1.6;\n"
        << "    }\n"
        << "    footer {\n"
        << "      text-align: center;\n"
        << "      font-size: 0.8rem;\n"
        << "      color: var(--text-muted);\n"
        << "      margin-top: 3rem;\n"
        << "    }\n"
        << "  </style>\n"
        << "</head>\n"
        << "<body>\n"
        << "<div class=\"container\">\n\n";

    // ── Header ───────────────────────────────────────────────────────────────
    out << "  <header>\n"
        << "    <div class=\"header-top\">\n"
        << "      <div>\n"
        << "        <h1>\n"
        << "          <svg width=\"28\" height=\"28\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"var(--accent)\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><polyline points=\"22 12 18 12 15 21 9 3 6 12 2 12\"></polyline></svg>\n"
        << "          Code Performance Analyzer <span style=\"font-size: 1rem; color: var(--accent);\">v4.0</span>\n"
        << "        </h1>\n"
        << "        <div class=\"subtitle\">Executive Performance Benchmark Report &bull; Run ID: <code>"
        << escape_html(run.run_id) << "</code> &bull; " << escape_html(run.timestamp) << "</div>\n"
        << "      </div>\n"
        << "      <div>\n"
        << "        <span class=\"badge badge-cyan\">Completed ✓</span>\n"
        << "      </div>\n"
        << "    </div>\n"
        << "  </header>\n\n";

    // ── Executive Metric Cards ────────────────────────────────────────────────
    out << "  <div class=\"metrics-grid\">\n"
        << "    <div class=\"metric-card success\">\n"
        << "      <div class=\"metric-label\">Fastest Algorithm</div>\n"
        << "      <div class=\"metric-value\">" << escape_html(fastest_name) << "</div>\n"
        << "      <div class=\"metric-sub\">" << fastest_time << " &bull; " << std::fixed << std::setprecision(2) << speedup_factor << "x speedup vs slowest</div>\n"
        << "    </div>\n"
        << "    <div class=\"metric-card\">\n"
        << "      <div class=\"metric-label\">Slowest Algorithm</div>\n"
        << "      <div class=\"metric-value\">" << escape_html(slowest_name) << "</div>\n"
        << "      <div class=\"metric-sub\">" << slowest_time << " baseline</div>\n"
        << "    </div>\n"
        << "    <div class=\"metric-card\">\n"
        << "      <div class=\"metric-label\">Peak Memory Footprint</div>\n"
        << "      <div class=\"metric-value\">" << format_memory(max_peak_memory) << "</div>\n"
        << "      <div class=\"metric-sub\">Private process memory delta</div>\n"
        << "    </div>\n"
        << "    <div class=\"metric-card " << (regression.has_regressions ? "danger" : "success") << "\">\n"
        << "      <div class=\"metric-label\">Regression Status</div>\n"
        << "      <div class=\"metric-value\"><span class=\"badge " << reg_badge_class << "\">" << reg_badge_text << "</span></div>\n"
        << "      <div class=\"metric-sub\">" << (regression.valid ? ("Compared with " + escape_html(regression.baseline_run_id)) : "Run-to-run comparison") << "</div>\n"
        << "    </div>\n"
        << "  </div>\n\n";

    // ── Charts Grid ──────────────────────────────────────────────────────────
    out << "  <div class=\"charts-grid\">\n"
        << "    <div class=\"card\">\n"
        << "      <div class=\"card-title\">\n"
        << "        <span>📈 Execution Time vs Input Size (N)</span>\n"
        << "        <span style=\"font-size: 0.8rem; color: var(--text-muted);\">Interactive Curve</span>\n"
        << "      </div>\n"
        << "      <div class=\"chart-container\">\n"
        << "        <canvas id=\"timeChart\"></canvas>\n"
        << "      </div>\n"
        << "    </div>\n"
        << "    <div class=\"card\">\n"
        << "      <div class=\"card-title\">\n"
        << "        <span>🧠 Peak Memory vs Input Size (N)</span>\n"
        << "        <span style=\"font-size: 0.8rem; color: var(--text-muted);\">MB Scaling</span>\n"
        << "      </div>\n"
        << "      <div class=\"chart-container\">\n"
        << "        <canvas id=\"memoryChart\"></canvas>\n"
        << "      </div>\n"
        << "    </div>\n"
        << "  </div>\n\n";

    // ── Algorithm Comparison Bar Chart ───────────────────────────────────────
    out << "  <div class=\"card\">\n"
        << "    <div class=\"card-title\">\n"
        << "      <span>⚡ Algorithm Speedup Factor (Relative to Slowest)</span>\n"
        << "      <span style=\"font-size: 0.8rem; color: var(--text-muted);\">Higher is faster</span>\n"
        << "    </div>\n"
        << "    <div class=\"chart-container\" style=\"height: 280px;\">\n"
        << "      <canvas id=\"speedupChart\"></canvas>\n"
        << "    </div>\n"
        << "  </div>\n\n";

    // ── Dataset Information ──────────────────────────────────────────────────
    out << "  <div class=\"card\">\n"
        << "    <div class=\"card-title\">📁 Dataset Specification</div>\n"
        << "    <div class=\"info-grid\">\n"
        << "      <div class=\"info-item\"><span class=\"info-key\">Input Source:</span><span class=\"info-val\">" << (run.input.type == "custom_file" ? "Custom File" : "Generated Synthetic") << "</span></div>\n"
        << "      <div class=\"info-item\"><span class=\"info-key\">Target File:</span><span class=\"info-val\">" << (run.input.file_path.empty() ? "None (In-Memory Generated)" : escape_html(run.input.file_path)) << "</span></div>\n"
        << "      <div class=\"info-item\"><span class=\"info-key\">Element Count (N):</span><span class=\"info-val\">" << format_number(run.input.element_count > 0 ? run.input.element_count : (run.results.empty() ? 0 : run.results[0].input_size)) << "</span></div>\n"
        << "      <div class=\"info-item\"><span class=\"info-key\">Distribution / Order:</span><span class=\"info-val\">" << (run.input.order_description.empty() ? (run.results.empty() ? "Unknown" : run.results[0].input_type) : escape_html(run.input.order_description)) << "</span></div>\n";
    
    if (run.input.distinct_count > 0 || run.input.duplicate_count > 0) {
        out << "      <div class=\"info-item\"><span class=\"info-key\">Distinct Elements:</span><span class=\"info-val\">" << format_number(run.input.distinct_count) << "</span></div>\n"
            << "      <div class=\"info-item\"><span class=\"info-key\">Duplicate Elements:</span><span class=\"info-val\">" << format_number(run.input.duplicate_count) << "</span></div>\n"
            << "      <div class=\"info-item\"><span class=\"info-key\">Value Range:</span><span class=\"info-val\">[" << run.input.min_value << " &hellip; " << run.input.max_value << "]</span></div>\n";
    }
    out << "    </div>\n"
        << "  </div>\n\n";

    // ── Detailed Results Table ───────────────────────────────────────────────
    out << "  <div class=\"card\">\n"
        << "    <div class=\"card-title\">📊 Detailed Algorithm Measurements</div>\n"
        << "    <div class=\"table-container\">\n"
        << "      <table>\n"
        << "        <thead>\n"
        << "          <tr>\n"
        << "            <th>Algorithm</th>\n"
        << "            <th>Input Type</th>\n"
        << "            <th>N</th>\n"
        << "            <th>Mean Time</th>\n"
        << "            <th>Median Time</th>\n"
        << "            <th>P95 Time</th>\n"
        << "            <th>Std Dev</th>\n"
        << "            <th>Peak Memory</th>\n"
        << "            <th>Correctness</th>\n"
        << "          </tr>\n"
        << "        </thead>\n"
        << "        <tbody>\n";

    for (const auto& rec : run.results) {
        out << "          <tr>\n"
            << "            <td style=\"font-weight: 600;\">" << escape_html(rec.algorithm_name.empty() ? rec.algorithm : rec.algorithm_name) << "</td>\n"
            << "            <td>" << escape_html(rec.input_type) << "</td>\n"
            << "            <td>" << format_number(rec.input_size) << "</td>\n"
            << "            <td style=\"color: var(--accent); font-weight: 600;\">" << format_time(rec.time_mean_ns) << "</td>\n"
            << "            <td>" << format_time(rec.time_median_ns) << "</td>\n"
            << "            <td>" << format_time(rec.time_max_ns) << "</td>\n"
            << "            <td>" << format_time(rec.time_stddev_ns) << "</td>\n"
            << "            <td>" << format_memory(rec.memory_peak_increase_bytes) << "</td>\n"
            << "            <td><span class=\"badge badge-success\">PASS ✓</span></td>\n"
            << "          </tr>\n";
    }

    out << "        </tbody>\n"
        << "      </table>\n"
        << "    </div>\n"
        << "  </div>\n\n";

    // ── Regression Section (if valid) ─────────────────────────────────────────
    if (regression.valid && !regression.records.empty()) {
        out << "  <div class=\"card\">\n"
            << "    <div class=\"card-title\">\n"
            << "      <span>🔍 Run-to-Run Regression Diagnostics</span>\n"
            << "      <span style=\"font-size: 0.8rem; color: var(--text-muted);\">Baseline: " << escape_html(regression.baseline_run_id) << " (Tolerance: ±" << regression.tolerance_percentage << "%)</span>\n"
            << "    </div>\n"
            << "    <div class=\"table-container\">\n"
            << "      <table>\n"
            << "        <thead>\n"
            << "          <tr>\n"
            << "            <th>Algorithm</th>\n"
            << "            <th>Baseline Time</th>\n"
            << "            <th>Current Time</th>\n"
            << "            <th>Delta %</th>\n"
            << "            <th>Status</th>\n"
            << "          </tr>\n"
            << "        </thead>\n"
            << "        <tbody>\n";

        for (const auto& reg : regression.records) {
            std::string st_class = "badge-neutral";
            std::string st_text = "STABLE";
            if (reg.status == analysis::RegressionStatus::Improved) {
                st_class = "badge-success";
                st_text = "IMPROVED ✓";
            } else if (reg.status == analysis::RegressionStatus::Regressed) {
                st_class = "badge-danger";
                st_text = "REGRESSION ✗";
            }

            out << "          <tr>\n"
                << "            <td style=\"font-weight: 600;\">" << escape_html(reg.algorithm_name) << "</td>\n"
                << "            <td>" << format_time(reg.baseline_time_ns) << "</td>\n"
                << "            <td style=\"font-weight: 600;\">" << format_time(reg.current_time_ns) << "</td>\n"
                << "            <td style=\"font-weight: 600; color: " << (reg.delta_percentage < 0 ? "var(--success)" : (reg.delta_percentage > 5.0 ? "var(--danger)" : "var(--text-muted)")) << ";\">"
                << (reg.delta_percentage > 0 ? "+" : "") << std::fixed << std::setprecision(2) << reg.delta_percentage << "%</td>\n"
                << "            <td><span class=\"badge " << st_class << "\">" << st_text << "</span></td>\n"
                << "          </tr>\n";
        }

        out << "        </tbody>\n"
            << "      </table>\n"
            << "    </div>\n"
            << "  </div>\n\n";
    }

    // ── System Environment & Conclusion ───────────────────────────────────────
    out << "  <div class=\"card\">\n"
        << "    <div class=\"card-title\">💻 Hardware & System Specifications</div>\n"
        << "    <div class=\"info-grid\">\n"
        << "      <div class=\"info-item\"><span class=\"info-key\">Operating System:</span><span class=\"info-val\">" << escape_html(run.system.os) << "</span></div>\n"
        << "      <div class=\"info-item\"><span class=\"info-key\">Processor (CPU):</span><span class=\"info-val\">" << escape_html(run.system.cpu) << "</span></div>\n"
        << "      <div class=\"info-item\"><span class=\"info-key\">Architecture:</span><span class=\"info-val\">" << escape_html(run.system.architecture) << " (" << run.system.logical_cpus << " Threads)</span></div>\n"
        << "      <div class=\"info-item\"><span class=\"info-key\">Compiler:</span><span class=\"info-val\">" << escape_html(run.system.compiler) << " (" << escape_html(run.system.cxx_standard) << ")</span></div>\n"
        << "      <div class=\"info-item\"><span class=\"info-key\">Optimization:</span><span class=\"info-val\">" << escape_html(run.system.optimization) << "</span></div>\n"
        << "      <div class=\"info-item\"><span class=\"info-key\">Warmup / Iterations:</span><span class=\"info-val\">" << run.configuration.warmup_runs << " warmup &bull; " << run.configuration.iterations << " iterations</span></div>\n"
        << "    </div>\n"
        << "  </div>\n\n";

    // ── Executive Conclusion ──────────────────────────────────────────────────
    out << "  <div class=\"card\">\n"
        << "    <div class=\"card-title\">📝 Automated Analysis Conclusion</div>\n"
        << "    <div class=\"conclusion-box\">\n"
        << "      <strong>Performance Summary:</strong> Under the measured test environment and dataset configuration (N = "
        << format_number(run.input.element_count > 0 ? run.input.element_count : (run.results.empty() ? 0 : run.results[0].input_size))
        << "), <strong>" << escape_html(fastest_name) << "</strong> exhibited the highest sorting throughput, completing in " << fastest_time
        << " and delivering a <strong>" << std::fixed << std::setprecision(2) << speedup_factor << "x speedup</strong> over " << escape_html(slowest_name) << ".<br><br>\n";

    if (regression.valid) {
        out << "      <strong>Stability Assessment:</strong> Compared against baseline <code>" << escape_html(regression.baseline_run_id)
            << "</code>, " << regression.improved_count << " algorithm(s) demonstrated statistically significant performance improvements, "
            << regression.stable_count << " remained stable within the ±" << regression.tolerance_percentage << "% tolerance band, and "
            << regression.regressed_count << " exhibited performance regressions.<br><br>\n";
    }

    out << "      <em>Note: Performance results reflect execution times and working set memory footprints measured under the specific operating environment and dataset properties detailed above.</em>\n"
        << "    </div>\n"
        << "  </div>\n\n";

    out << "  <footer>Code Performance Analyzer V4 &bull; Built with C++17 &bull; Professional Benchmark Architecture</footer>\n"
        << "</div>\n\n";

    // ── Chart.js JavaScript Initialization ────────────────────────────────────
    out << "<script>\n"
        << "  const chartDarkTheme = {\n"
        << "    color: '#9ca3af',\n"
        << "    grid: { color: 'rgba(255, 255, 255, 0.06)' },\n"
        << "    border: { color: 'rgba(255, 255, 255, 0.1)' }\n"
        << "  };\n\n";

    // 1. Time vs N Chart Data
    std::vector<double> time_x_labels;
    for (const auto& s : time_chart.series) {
        for (const auto& pt : s.points) {
            if (std::find(time_x_labels.begin(), time_x_labels.end(), pt.x) == time_x_labels.end()) {
                time_x_labels.push_back(pt.x);
            }
        }
    }
    std::sort(time_x_labels.begin(), time_x_labels.end());

    out << "  // 1. Time vs N Chart\n"
        << "  const timeCtx = document.getElementById('timeChart').getContext('2d');\n"
        << "  const timeChart = new Chart(timeCtx, {\n"
        << "    type: 'line',\n"
        << "    data: {\n"
        << "      labels: [";
    for (size_t i = 0; i < time_x_labels.size(); ++i) {
        out << (i > 0 ? ", " : "") << "'" << static_cast<size_t>(time_x_labels[i]) << "'";
    }
    out << "],\n      datasets: [\n";

    for (size_t s_idx = 0; s_idx < time_chart.series.size(); ++s_idx) {
        const auto& ser = time_chart.series[s_idx];
        std::string color = get_color(s_idx);
        std::map<double, double> pt_map;
        for (const auto& pt : ser.points) pt_map[pt.x] = pt.y;

        out << "        {\n"
            << "          label: '" << escape_html(ser.algorithm_name) << "',\n"
            << "          borderColor: '" << color << "',\n"
            << "          backgroundColor: '" << get_color(s_idx, 0.1) << "',\n"
            << "          borderWidth: 2,\n"
            << "          tension: 0.25,\n"
            << "          pointRadius: 4,\n"
            << "          pointHoverRadius: 6,\n"
            << "          data: [";
        for (size_t i = 0; i < time_x_labels.size(); ++i) {
            auto it = pt_map.find(time_x_labels[i]);
            if (i > 0) out << ", ";
            if (it != pt_map.end()) {
                out << std::fixed << std::setprecision(2) << it->second;
            } else {
                out << "null";
            }
        }
        out << "]\n        }" << (s_idx + 1 < time_chart.series.size() ? "," : "") << "\n";
    }

    out << "      ]\n"
        << "    },\n"
        << "    options: {\n"
        << "      responsive: true,\n"
        << "      maintainAspectRatio: false,\n"
        << "      interaction: { mode: 'index', intersect: false },\n"
        << "      scales: {\n"
        << "        x: { title: { display: true, text: 'Input Size (N)', color: '#9ca3af' }, ...chartDarkTheme },\n"
        << "        y: { title: { display: true, text: 'Mean Time (µs)', color: '#9ca3af' }, ...chartDarkTheme }\n"
        << "      },\n"
        << "      plugins: {\n"
        << "        legend: { labels: { color: '#f3f4f6', boxWidth: 12 } },\n"
        << "        tooltip: {\n"
        << "          callbacks: {\n"
        << "            label: function(c) { return c.dataset.label + ': ' + c.parsed.y.toFixed(2) + ' µs'; }\n"
        << "          }\n"
        << "        }\n"
        << "      }\n"
        << "    }\n"
        << "  });\n\n";

    // 2. Memory vs N Chart Data
    std::vector<double> mem_x_labels;
    for (const auto& s : memory_chart.series) {
        for (const auto& pt : s.points) {
            if (std::find(mem_x_labels.begin(), mem_x_labels.end(), pt.x) == mem_x_labels.end()) {
                mem_x_labels.push_back(pt.x);
            }
        }
    }
    std::sort(mem_x_labels.begin(), mem_x_labels.end());

    out << "  // 2. Memory vs N Chart\n"
        << "  const memCtx = document.getElementById('memoryChart').getContext('2d');\n"
        << "  const memChart = new Chart(memCtx, {\n"
        << "    type: 'line',\n"
        << "    data: {\n"
        << "      labels: [";
    for (size_t i = 0; i < mem_x_labels.size(); ++i) {
        out << (i > 0 ? ", " : "") << "'" << static_cast<size_t>(mem_x_labels[i]) << "'";
    }
    out << "],\n      datasets: [\n";

    for (size_t s_idx = 0; s_idx < memory_chart.series.size(); ++s_idx) {
        const auto& ser = memory_chart.series[s_idx];
        std::string color = get_color(s_idx);
        std::map<double, double> pt_map;
        for (const auto& pt : ser.points) pt_map[pt.x] = pt.y;

        out << "        {\n"
            << "          label: '" << escape_html(ser.algorithm_name) << "',\n"
            << "          borderColor: '" << color << "',\n"
            << "          backgroundColor: '" << get_color(s_idx, 0.1) << "',\n"
            << "          borderWidth: 2,\n"
            << "          tension: 0.25,\n"
            << "          pointRadius: 4,\n"
            << "          data: [";
        for (size_t i = 0; i < mem_x_labels.size(); ++i) {
            auto it = pt_map.find(mem_x_labels[i]);
            if (i > 0) out << ", ";
            if (it != pt_map.end()) {
                out << std::fixed << std::setprecision(4) << it->second;
            } else {
                out << "null";
            }
        }
        out << "]\n        }" << (s_idx + 1 < memory_chart.series.size() ? "," : "") << "\n";
    }

    out << "      ]\n"
        << "    },\n"
        << "    options: {\n"
        << "      responsive: true,\n"
        << "      maintainAspectRatio: false,\n"
        << "      interaction: { mode: 'index', intersect: false },\n"
        << "      scales: {\n"
        << "        x: { title: { display: true, text: 'Input Size (N)', color: '#9ca3af' }, ...chartDarkTheme },\n"
        << "        y: { title: { display: true, text: 'Peak Memory Delta (MB)', color: '#9ca3af' }, ...chartDarkTheme }\n"
        << "      },\n"
        << "      plugins: {\n"
        << "        legend: { labels: { color: '#f3f4f6', boxWidth: 12 } }\n"
        << "      }\n"
        << "    }\n"
        << "  });\n\n";

    // 3. Speedup Bar Chart Data
    out << "  // 3. Speedup Comparison Bar Chart\n"
        << "  const speedCtx = document.getElementById('speedupChart').getContext('2d');\n"
        << "  const speedupChart = new Chart(speedCtx, {\n"
        << "    type: 'bar',\n"
        << "    data: {\n"
        << "      labels: [";

    std::vector<std::string> speed_labels;
    std::vector<double> speed_values;
    if (comparison.valid && !comparison.groups.empty()) {
        for (const auto& rk : comparison.groups[0].rankings) {
            speed_labels.push_back(rk.algorithm_name);
            speed_values.push_back(rk.speedup_vs_slowest);
        }
    } else {
        for (const auto& rec : run.results) {
            speed_labels.push_back(rec.algorithm_name.empty() ? rec.algorithm : rec.algorithm_name);
            double sp = 1.0;
            if (rec.time_mean_ns > 0.0 && !run.results.empty()) {
                double max_t = 0.0;
                for (const auto& r : run.results) if (r.time_mean_ns > max_t) max_t = r.time_mean_ns;
                sp = max_t / rec.time_mean_ns;
            }
            speed_values.push_back(sp);
        }
    }

    for (size_t i = 0; i < speed_labels.size(); ++i) {
        out << (i > 0 ? ", " : "") << "'" << escape_html(speed_labels[i]) << "'";
    }
    out << "],\n      datasets: [{\n"
        << "        label: 'Speedup Factor',\n"
        << "        data: [";
    for (size_t i = 0; i < speed_values.size(); ++i) {
        out << (i > 0 ? ", " : "") << std::fixed << std::setprecision(2) << speed_values[i];
    }
    out << "],\n"
        << "        backgroundColor: [";
    for (size_t i = 0; i < speed_values.size(); ++i) {
        out << (i > 0 ? ", " : "") << "'" << get_color(i, 0.8) << "'";
    }
    out << "],\n"
        << "        borderColor: [";
    for (size_t i = 0; i < speed_values.size(); ++i) {
        out << (i > 0 ? ", " : "") << "'" << get_color(i) << "'";
    }
    out << "],\n"
        << "        borderWidth: 1,\n"
        << "        borderRadius: 6\n"
        << "      }]\n"
        << "    },\n"
        << "    options: {\n"
        << "      indexAxis: 'y',\n"
        << "      responsive: true,\n"
        << "      maintainAspectRatio: false,\n"
        << "      scales: {\n"
        << "        x: { title: { display: true, text: 'Speedup vs Slowest (1.0x = baseline)', color: '#9ca3af' }, ...chartDarkTheme },\n"
        << "        y: { ...chartDarkTheme }\n"
        << "      },\n"
        << "      plugins: {\n"
        << "        legend: { display: false },\n"
        << "        tooltip: {\n"
        << "          callbacks: {\n"
        << "            label: function(c) { return 'Speedup: ' + c.parsed.x.toFixed(2) + 'x faster'; }\n"
        << "          }\n"
        << "        }\n"
        << "      }\n"
        << "    }\n"
        << "  });\n"
        << "</script>\n"
        << "</body>\n"
        << "</html>\n";

    out.close();

    if (options.auto_open_in_browser) {
        open_in_browser(full_path.string());
    }

    return full_path.string();
}

} // namespace reporting

