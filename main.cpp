#include "BenchmarkRunner.h"
#include "benchmarks/RegisterBenchmarks.h"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
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
        << "Usage:\n"
        << "  analyzer.exe --all [options]\n"
        << "  analyzer.exe --quick [options]      Fast smoke test of all benchmarks\n"
        << "  analyzer.exe --quicksort [options]  Run Quick Sort only\n\n"
        << "Options:\n"
        << "  --sizes <a,b,c>       Input sizes\n"
        << "  --cases <list>        random,sorted,reverse,nearly-sorted,many-duplicates,all-equal\n"
        << "  --iterations <n>      Measurement runs\n"
        << "  --warmup <n>          Warm-up runs\n"
        << "  --seed <n>            Reproducible random seed\n"
        << "  --no-rdtsc            Disable RDTSC timing\n"
        << "  --no-qpc              Disable QueryPerformanceCounter timing\n"
        << "  --cpu <n>             Pin benchmark thread to logical CPU n\n"
        << "  --no-memory           Disable process memory measurement\n"
        << "  --help                Show this help\n\n";
    runner.list_benchmarks();
}

} // namespace

int main(int argc, char* argv[]) {
    #ifdef _WIN32
SetConsoleOutputCP(CP_UTF8);
SetConsoleCP(CP_UTF8);
#endif
    BenchmarkConfig config;
    std::string selected_benchmark;
    bool all_selected = false;
    bool quick_mode = false;

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

    try {
        if (quick_mode) {
            if (!sizes_explicit) {
                config.input_sizes = {100, 1000};
            }
            if (!cases_explicit) {
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
            return 0;
        }

        // --all, --quick, and no explicit selection all run the full suite.
        (void)all_selected;
        runner.run_all();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Benchmark error: " << error.what() << '\n';
        return 1;
    }
}
