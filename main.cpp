#include "BenchmarkRunner.h"
#include "FileInputLoader.h"
#include "analysis/HistoryManager.h"
#include "analysis/ComparisonAnalyzer.h"
#include "analysis/RegressionAnalyzer.h"
#include "analysis/TrendAnalyzer.h"
#include "reporting/HtmlReportGenerator.h"
#include "server/DashboardServer.h"
#include "benchmarks/RegisterBenchmarks.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif
namespace {

bool parse_sizes(const std::string& text, std::vector<size_t>& sizes) {
    sizes.clear();
    std::stringstream stream(text);
    std::string part;

    while (std::getline(stream, part, ',')) {
        if (part.empty()) {
            return false;
        }

        try {
            size_t position = 0;
            const unsigned long long value = std::stoull(part, &position);
            if (position != part.size() || value == 0) {
                return false;
            }

            const size_t converted = static_cast<size_t>(value);
            if (static_cast<unsigned long long>(converted) != value) {
                return false;
            }
            sizes.push_back(converted);
        } catch (...) {
            return false;
        }
    }

    // Duplicate input sizes add no information and distort complexity fitting.
    std::sort(sizes.begin(), sizes.end());
    sizes.erase(std::unique(sizes.begin(), sizes.end()), sizes.end());
    return !sizes.empty();
}

std::string trim_copy(std::string value) {
    const auto not_space = [](unsigned char c) {
        return c != ' ' && c != '\t' && c != '\r' && c != '\n';
    };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

bool parse_cases(const std::string& text, std::vector<InputDataCase>& cases) {
    cases.clear();
    std::stringstream stream(text);
    std::string part;

    while (std::getline(stream, part, ',')) {
        part = trim_copy(part);
        if (part == "random") {
            cases.push_back(InputDataCase::Random);
        } else if (part == "sorted") {
            cases.push_back(InputDataCase::Sorted);
        } else if (part == "reverse" || part == "reverse-sorted") {
            cases.push_back(InputDataCase::ReverseSorted);
        } else if (part == "nearly-sorted" || part == "nearly_sorted") {
            cases.push_back(InputDataCase::NearlySorted);
        } else if (part == "many-duplicates" || part == "many_duplicates") {
            cases.push_back(InputDataCase::ManyDuplicates);
        } else if (part == "all-equal" || part == "all_equal") {
            cases.push_back(InputDataCase::AllEqual);
        } else {
            return false;
        }
    }

    std::sort(cases.begin(), cases.end(), [](InputDataCase left, InputDataCase right) {
        return static_cast<int>(left) < static_cast<int>(right);
    });
    cases.erase(std::unique(cases.begin(), cases.end()), cases.end());
    return !cases.empty();
}

bool parse_nonnegative_int(const std::string& text, int& value) {
    try {
        size_t position = 0;
        const long parsed = std::stol(text, &position);
        if (position != text.size() || parsed < 0 || parsed > 1000000L) {
            return false;
        }
        value = static_cast<int>(parsed);
        return true;
    } catch (...) {
        return false;
    }
}

bool parse_positive_int(const std::string& text, int& value) {
    if (!parse_nonnegative_int(text, value) || value <= 0) {
        return false;
    }
    return true;
}

bool parse_seed(const std::string& text, uint32_t& seed) {
    try {
        size_t position = 0;
        const unsigned long long parsed = std::stoull(text, &position);
        if (position != text.size() || parsed > std::numeric_limits<uint32_t>::max()) {
            return false;
        }
        seed = static_cast<uint32_t>(parsed);
        return true;
    } catch (...) {
        return false;
    }
}

void print_usage(const BenchmarkRunner& runner) {
    std::cout
        << "Code Performance Analyzer v3.0\n\n"
        << "Graphical Mode (Default):\n"
        << "  analyzer.exe                        Launch Graphical Web Dashboard on port 8080\n"
        << "  analyzer.exe --gui [options]        Launch Graphical Web Dashboard\n"
        << "  analyzer.exe --web [options]        Alias for --gui\n"
        << "  analyzer.exe --server [options]     Alias for --gui\n\n"
        << "Dashboard Server Options:\n"
        << "  --port <p>          Set server port (default: 8080, auto-fallbacks to 8081)\n"
        << "  --no-browser        Start server without opening web browser automatically\n\n"
        << "Terminal Benchmark Mode:\n"
        << "  analyzer.exe --all [options]        Run full sorting benchmark suite in terminal\n"
        << "  analyzer.exe --quick [options]      Fast smoke test of all benchmarks\n"
        << "  analyzer.exe --quicksort [options]  Run Quick Sort only\n"
        << "  analyzer.exe --cli [options]        Explicit CLI benchmark execution\n\n"
        << "Terminal Benchmark Options:\n"
        << "  --sizes <a,b,c>       Input sizes\n"
        << "  --cases <list>        random,sorted,reverse,nearly-sorted,many-duplicates,all-equal\n"
        << "  --file <path>         Load custom integer dataset from text file\n"
        << "  --iterations <n>      Measurement runs\n"
        << "  --warmup <n>          Warm-up runs\n"
        << "  --seed <n>            Reproducible random seed\n"
        << "  --no-rdtsc            Disable RDTSC timing\n"
        << "  --no-qpc              Disable QueryPerformanceCounter timing\n"
        << "  --cpu <n>             Pin benchmark thread to logical CPU n\n"
        << "  --no-memory           Disable process memory measurement\n"
        << "  --history [run_id]    List benchmark history or view specific run details\n"
        << "  --compare [run_id]    Compare algorithms from latest run or a specific historical run\n"
        << "  --regression [run_id] Compare latest run against baseline (or previous compatible run)\n"
        << "  --trend [dist]        Time vs Input Size table from history (dist: random,sorted,...)\n"
        << "  --trend-memory [dist] Peak Memory vs Input Size table from history\n"
        << "  --report [run_id]     Generate and open interactive HTML report with Chart.js graphs\n"
        << "  --help, -h            Show this help\n\n";
    runner.list_benchmarks();
}

static std::string resolve_web_root() {
    if (std::filesystem::exists("web/index.html")) {
        return "web";
    }
#ifdef _WIN32
    char exe_path[MAX_PATH];
    DWORD len = GetModuleFileNameA(NULL, exe_path, MAX_PATH);
    if (len > 0 && len < MAX_PATH) {
        std::filesystem::path p(exe_path);
        std::filesystem::path candidate = p.parent_path() / "web";
        if (std::filesystem::exists(candidate / "index.html")) {
            return candidate.string();
        }
    }
#endif
    return "web";
}

} // namespace

#ifdef _WIN32
static server::DashboardServer* g_active_server = nullptr;

BOOL WINAPI console_ctrl_handler(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_BREAK_EVENT || signal == CTRL_CLOSE_EVENT) {
        if (g_active_server) {
            std::cout << "\nStopping dashboard server...\n" << std::flush;
            g_active_server->stop();
        }
        return TRUE;
    }
    return FALSE;
}
#endif

int main(int argc, char* argv[]) {
    #ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    #endif

    // Check for help flag before anything else
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h" || arg == "/?") {
            BenchmarkConfig config;
            BenchmarkRunner help_runner(config);
            register_all_benchmarks(help_runner);
            print_usage(help_runner);
            return 0;
        }
    }

    // Check if graphical dashboard mode is requested (or default if no args)
    bool is_gui_request = (argc == 1);
    int gui_port = 8080;
    bool open_browser = true;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--gui" || arg == "--web" || arg == "--server" || arg == "--dashboard") {
            is_gui_request = true;
        } else if (arg == "--no-browser") {
            open_browser = false;
        } else if (arg == "--port") {
            is_gui_request = true;
            if (i + 1 < argc) {
                try {
                    gui_port = std::stoi(argv[++i]);
                } catch (...) {
                    std::cerr << "Error: Invalid port number: " << argv[i] << "\n";
                    return 1;
                }
            }
        }
    }

    if (is_gui_request) {
        std::string web_dir = resolve_web_root();
        server::DashboardServer srv(gui_port, web_dir);
#ifdef _WIN32
        g_active_server = &srv;
        SetConsoleCtrlHandler(console_ctrl_handler, TRUE);
#endif
        if (!srv.start()) {
            std::cerr << "Error: Failed to start Dashboard server on port " << gui_port << " or fallback port.\n" << std::flush;
            return 1;
        }

        std::cout << "\n";
        std::cout << "╔══════════════════════════════════════════════════════════════════════╗\n";
        std::cout << "║               CODE PERFORMANCE ANALYZER V4 DASHBOARD                 ║\n";
        std::cout << "╠══════════════════════════════════════════════════════════════════════╣\n";
        std::cout << "║  Server URL        : " << srv.url() << "\n";
        std::cout << "║  Web Assets Root   : " << web_dir << "\n";
        if (open_browser) {
            std::cout << "║  Browser Launch    : Opening in your default web browser...          ║\n";
        } else {
            std::cout << "║  Browser Launch    : Disabled (--no-browser)                         ║\n";
        }
        std::cout << "║  Press Ctrl+C in terminal to stop dashboard server.                  ║\n";
        std::cout << "╚══════════════════════════════════════════════════════════════════════╝\n\n" << std::flush;

        if (open_browser) {
#ifdef _WIN32
            ShellExecuteA(NULL, "open", srv.url().c_str(), NULL, NULL, SW_SHOWNORMAL);
#else
            std::string cmd = "xdg-open " + srv.url() + " > /dev/null 2>&1 &";
            (void)system(cmd.c_str());
#endif
        }

        srv.wait();
        return 0;
    }

    BenchmarkConfig config;
    std::string selected_benchmark;
    std::string file_path;
    bool all_selected = false;
    bool quick_mode = false;
    bool generate_html_report = false;

    bool sizes_explicit = false;
    bool cases_explicit = false;
    bool iterations_explicit = false;
    bool warmup_explicit = false;

    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];

        if (argument == "--help") {
            // BenchmarkRunner is only needed for its registered-benchmark help list.
            BenchmarkRunner help_runner(config);
            register_all_benchmarks(help_runner);
            print_usage(help_runner);
            return 0;
        }

        if (argument == "--cli") {
            continue;
        }

        if (argument == "--all") {
            if (quick_mode || !selected_benchmark.empty()) {
                std::cerr << "Error: --all cannot be combined with --quick or a selected benchmark.\n";
                return 1;
            }
            all_selected = true;
            continue;
        }

        if (argument == "--quick") {
            if (all_selected || !selected_benchmark.empty()) {
                std::cerr << "Error: --quick cannot be combined with --all or a selected benchmark.\n";
                return 1;
            }
            quick_mode = true;
            continue;
        }

        if (argument == "--sizes" || argument == "--cases" ||
            argument == "--iterations" || argument == "--warmup" ||
            argument == "--seed" || argument == "--cpu") {
            if (i + 1 >= argc) {
                std::cerr << "Error: " << argument << " requires a value.\n";
                return 1;
            }
            const std::string value = argv[++i];

            if (argument == "--sizes") {
                if (!parse_sizes(value, config.input_sizes)) {
                    std::cerr << "Error: invalid input sizes: " << value << '\n';
                    return 1;
                }
                sizes_explicit = true;
            } else if (argument == "--cases") {
                if (!parse_cases(value, config.input_cases)) {
                    std::cerr << "Error: invalid input cases: " << value << '\n';
                    return 1;
                }
                cases_explicit = true;
            } else if (argument == "--iterations") {
                if (!parse_positive_int(value, config.iterations)) {
                    std::cerr << "Error: invalid iteration count: " << value << '\n';
                    return 1;
                }
                iterations_explicit = true;
            } else if (argument == "--warmup") {
                if (!parse_nonnegative_int(value, config.warmup_runs)) {
                    std::cerr << "Error: invalid warm-up count: " << value << '\n';
                    return 1;
                }
                warmup_explicit = true;
            } else if (argument == "--seed") {
                if (!parse_seed(value, config.random_seed)) {
                    std::cerr << "Error: invalid seed: " << value << '\n';
                    return 1;
                }
            } else if (argument == "--cpu") {
                int cpu = -1;
                if (!parse_nonnegative_int(value, cpu)) {
                    std::cerr << "Error: invalid CPU affinity index: " << value << '\n';
                    return 1;
                }
                config.cpu_affinity = cpu;
            }
            continue;
        }

        if (argument == "--no-rdtsc") {
            config.use_rdtsc = false;
            continue;
        }

        if (argument == "--no-qpc") {
            config.use_high_resolution_timer = false;
            continue;
        }

        if (argument == "--no-memory") {
            config.measure_memory = false;
            continue;
        }

        if (argument == "--file") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --file requires a filepath.\n";
                return 1;
            }
            file_path = argv[++i];
            continue;
        }

        if (argument == "--history") {
            analysis::HistoryManager history;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                const std::string target = argv[++i];
                const auto res = history.load_run(target);
                if (!res.success) {
                    std::cerr << "Error: " << res.error_message << '\n';
                    return 1;
                }
                history.print_run_details(res.run);
                return 0;
            } else {
                const auto runs = history.list_runs();
                history.print_history_table(runs);
                return 0;
            }
        }

        if (argument == "--compare") {
            analysis::HistoryManager history;
            analysis::BenchmarkRun run_to_compare;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                const std::string target = argv[++i];
                const auto res = history.load_run(target);
                if (!res.success) {
                    std::cerr << "Error: " << res.error_message << '\n';
                    return 1;
                }
                run_to_compare = res.run;
            } else {
                const auto latest = history.get_latest_run();
                if (!latest.has_value()) {
                    std::cerr << "Error: No benchmark history found to compare. Run a benchmark first.\n";
                    return 1;
                }
                run_to_compare = *latest;
            }

            const auto report = analysis::ComparisonAnalyzer::compare_run(run_to_compare);
            analysis::ComparisonAnalyzer::print_comparison_report(report);
            return 0;
        }

        if (argument == "--regression") {
            analysis::HistoryManager history;
            const auto latest_opt = history.get_latest_run();
            if (!latest_opt.has_value()) {
                std::cerr << "Error: No benchmark history found for regression analysis. Run a benchmark first.\n";
                return 1;
            }
            const auto current_run = *latest_opt;

            analysis::BenchmarkRun baseline_run;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                const std::string target = argv[++i];
                const auto res = history.load_run(target);
                if (!res.success) {
                    std::cerr << "Error: " << res.error_message << '\n';
                    return 1;
                }
                baseline_run = res.run;
            } else {
                const auto all_runs = history.list_runs();
                bool found_compatible = false;
                for (const auto& meta : all_runs) {
                    if (meta.run_id == current_run.run_id) continue;
                    const auto loaded = history.load_run(meta.run_id);
                    if (loaded.success && analysis::RegressionAnalyzer::are_runs_compatible(loaded.run, current_run)) {
                        baseline_run = loaded.run;
                        found_compatible = true;
                        break;
                    }
                }
                if (!found_compatible) {
                    const auto prev_opt = history.get_latest_run(current_run.run_id);
                    if (!prev_opt.has_value()) {
                        std::cerr << "Error: Need at least 2 historical runs to perform regression analysis.\n";
                        return 1;
                    }
                    baseline_run = *prev_opt;
                }
            }

            const auto report = analysis::RegressionAnalyzer::compare_runs(current_run, baseline_run);
            analysis::RegressionAnalyzer::print_regression_report(report);
            if (!report.valid) {
                return 1;
            }
            return 0;
        }

        if (argument == "--trend" || argument == "--trend-memory") {
            const bool memory_mode = (argument == "--trend-memory");

            // Optional distribution argument (e.g. "random", "sorted")
            analysis::ChartFilter filter;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                const std::string dist_arg = argv[++i];
                if      (dist_arg == "random")                               filter.input_distribution = "Random";
                else if (dist_arg == "sorted")                               filter.input_distribution = "Sorted";
                else if (dist_arg == "reverse" || dist_arg == "reverse-sorted") filter.input_distribution = "Reverse Sorted";
                else if (dist_arg == "nearly-sorted" || dist_arg == "nearly_sorted") filter.input_distribution = "Nearly Sorted";
                else if (dist_arg == "many-duplicates" || dist_arg == "many_duplicates") filter.input_distribution = "Many Duplicates";
                else if (dist_arg == "all-equal" || dist_arg == "all_equal") filter.input_distribution = "All Equal";
                else {
                    std::cerr << "Error: unknown distribution '" << dist_arg
                              << "' for " << argument << ".\n"
                              << "Valid values: random, sorted, reverse, nearly-sorted, many-duplicates, all-equal\n";
                    return 1;
                }
            }

            // Load all historical runs
            analysis::HistoryManager history;
            const auto metas = history.list_runs();
            if (metas.empty()) {
                std::cerr << "Error: No benchmark history found. Run a benchmark first.\n";
                return 1;
            }

            std::vector<analysis::BenchmarkRun> all_runs;
            all_runs.reserve(metas.size());
            for (const auto& meta : metas) {
                const auto res = history.load_run(meta.run_id);
                if (res.success) {
                    all_runs.push_back(res.run);
                }
            }

            if (memory_mode) {
                const auto chart = analysis::TrendAnalyzer::build_memory_vs_size(all_runs, filter);
                analysis::TrendAnalyzer::print_memory_table(chart);
                return chart.valid ? 0 : 1;
            } else {
                const auto chart = analysis::TrendAnalyzer::build_time_vs_size(all_runs, filter);
                analysis::TrendAnalyzer::print_time_table(chart);
                return chart.valid ? 0 : 1;
            }
        }

        if (argument == "--report") {
            // Case 1: Standalone with target run ID
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                const std::string target_id = argv[++i];
                analysis::HistoryManager history;
                const auto res = history.load_run(target_id);
                if (!res.success) {
                    std::cerr << "Error: " << res.error_message << "\n";
                    return 1;
                }
                std::vector<analysis::BenchmarkRun> all_runs;
                for (const auto& m : history.list_runs()) {
                    auto r = history.load_run(m.run_id);
                    if (r.success) all_runs.push_back(r.run);
                }
                reporting::ReportOptions r_opts;
                r_opts.auto_open_in_browser = true;
                std::string path = reporting::HtmlReportGenerator::generate_from_run(res.run, all_runs, r_opts);
                std::cout << "\nHTML report generated: " << path << "\n";
                std::cout << "Opened report in default web browser.\n\n";
                return 0;
            }

            // Case 2: Standalone without argument (e.g. `analyzer.exe --report`)
            if (argc == 2) {
                analysis::HistoryManager history;
                auto latest = history.get_latest_run();
                if (!latest.has_value()) {
                    std::cerr << "Error: No benchmark history found. Run a benchmark first.\n";
                    return 1;
                }
                std::vector<analysis::BenchmarkRun> all_runs;
                for (const auto& m : history.list_runs()) {
                    auto r = history.load_run(m.run_id);
                    if (r.success) all_runs.push_back(r.run);
                }
                reporting::ReportOptions r_opts;
                r_opts.auto_open_in_browser = true;
                std::string path = reporting::HtmlReportGenerator::generate_from_run(*latest, all_runs, r_opts);
                std::cout << "\nHTML report generated: " << path << "\n";
                std::cout << "Opened report in default web browser.\n\n";
                return 0;
            }

            // Case 3: Flag combined with benchmark run (`analyzer.exe --all --report`)
            generate_html_report = true;
            continue;
        }

        if (argument.size() > 2 && argument.rfind("--", 0) == 0) {
            if (all_selected || quick_mode || !selected_benchmark.empty()) {
                std::cerr << "Error: multiple benchmark selections were specified.\n";
                return 1;
            }
            selected_benchmark = argument.substr(2);
            continue;
        }

        std::cerr << "Unknown argument: " << argument << "\n\n";
        BenchmarkRunner help_runner(config);
        register_all_benchmarks(help_runner);
        print_usage(help_runner);
        return 1;
    }

    if (!file_path.empty()) {
        const ValidationResult val_result = FileInputLoader::load_and_validate(file_path);
        FileInputLoader::print_validation_card(val_result);
        if (!val_result.valid) {
            return 1;
        }

        FileInputLoader::print_dataset_properties(val_result.info, file_path, val_result.data);

        config.is_custom_file = true;
        config.custom_file_path = file_path;
        if (!cases_explicit) {
            config.input_cases = { InputDataCase::CustomFile };
        }
        if (!sizes_explicit) {
            config.input_sizes = { val_result.info.element_count };
        }
        FileInputLoader::set_active_dataset(
            std::move(val_result.data), val_result.info, file_path
        );
    }

    try {
        if (quick_mode) {
            if (!sizes_explicit && !config.is_custom_file) {
                config.input_sizes = {100, 1000};
            }
            if (!cases_explicit && !config.is_custom_file) {
                config.input_cases = {InputDataCase::Random};
            }
            if (!iterations_explicit) {
                config.iterations = 10;
            }
            if (!warmup_explicit) {
                config.warmup_runs = 2;
            }
        }

        BenchmarkRunner runner(config);
        register_all_benchmarks(runner);

        if (!selected_benchmark.empty()) {
            if (!runner.run_selected(selected_benchmark)) {
                std::cerr << "Unknown benchmark: --" << selected_benchmark << "\n\n";
                runner.list_benchmarks();
                return 1;
            }
        } else {
            // --all, --quick, and no explicit selection all run the full suite.
            (void)all_selected;
            runner.run_all();
        }

        if (generate_html_report) {
            analysis::HistoryManager history;
            std::vector<analysis::BenchmarkRun> all_runs;
            for (const auto& m : history.list_runs()) {
                auto r = history.load_run(m.run_id);
                if (r.success) all_runs.push_back(r.run);
            }
            reporting::ReportOptions r_opts;
            r_opts.auto_open_in_browser = true;
            std::string r_path = reporting::HtmlReportGenerator::generate_from_run(
                runner.last_analysis_run(), all_runs, r_opts
            );
            std::cout << "\nHTML report         : " << r_path << "\n";
            std::cout << "Opened report in default web browser.\n\n";
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Benchmark error: " << error.what() << '\n';
        return 1;
    }
}
