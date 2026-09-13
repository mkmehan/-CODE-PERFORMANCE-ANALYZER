#include "BenchmarkRunner.h"

#include "ComplexityAnalyzer.h"
#include "Statistics.h"
#include "SystemInfo.h"
#include "CpuAffinity.h"
#include "FileInputLoader.h"
#include "analysis/HistoryManager.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace {

const char* const divider =
    "──────────────────────────────────────────────────────────────────────";

struct SystemSnapshot {
    std::string os;
    std::string cpu;
    std::string architecture;
    unsigned int logical_cpus;
    unsigned int physical_cores;
    std::string compiler;
    std::string cxx_standard;
    std::string optimization;
};

struct ReportPaths {
    std::filesystem::path csv;
    std::filesystem::path json;
};

std::string comma_number(size_t value) {
    std::string result = std::to_string(value);
    for (int position = static_cast<int>(result.size()) - 3; position > 0; position -= 3) {
        result.insert(static_cast<size_t>(position), ",");
    }
    return result;
}

std::string format_time(double nanoseconds) {
    std::ostringstream output;
    output << std::fixed << std::setprecision(2);
    if (nanoseconds < 0.0) {
        output << "-";
        nanoseconds = -nanoseconds;
    }
    if (nanoseconds >= 1000000000.0) {
        output << nanoseconds / 1000000000.0 << " s";
    } else if (nanoseconds >= 1000000.0) {
        output << nanoseconds / 1000000.0 << " ms";
    } else if (nanoseconds >= 1000.0) {
        output << nanoseconds / 1000.0 << " µs";
    } else {
        output << nanoseconds << " ns";
    }
    return output.str();
}

std::string format_mb(double bytes) {
    const double mb = bytes / (1024.0 * 1024.0);
    const double rounded = (std::abs(mb) < 0.005) ? 0.0 : mb;
    std::ostringstream output;
    output << std::fixed << std::setprecision(2) << rounded << " MB";
    return output.str();
}

std::string format_cycles(double cycles) {
    std::ostringstream output;
    output << std::fixed << std::setprecision(0) << cycles << " cycles";
    return output.str();
}

std::string format_measurement(double value, const BenchmarkConfig& config) {
    return config.use_high_resolution_timer ? format_time(value) : format_cycles(value);
}

std::string verification_status(const BenchmarkSummary& summary) {
    if (!summary.verification_performed) {
        return "NOT CHECKED";
    }
    return summary.verified ? "PASS ✓" : "FAIL";
}

const BenchmarkStatistics& timing_statistics(
    const BenchmarkSummary& summary,
    const BenchmarkConfig& config
) {
    return config.use_high_resolution_timer ? summary.time : summary.cycles;
}

std::string join_cases(const std::vector<InputDataCase>& cases) {
    std::ostringstream output;
    for (size_t index = 0; index < cases.size(); ++index) {
        if (index > 0) {
            output << ", ";
        }
        output << input_data_case_name(cases[index]);
    }
    return output.str();
}

std::string timer_name(const BenchmarkConfig& config) {
    if (config.use_rdtsc && config.use_high_resolution_timer) {
        return "RDTSC + High Resolution Clock";
    }
    return config.use_rdtsc ? "RDTSC" : "High Resolution Clock";
}

uint32_t mix_seed(uint32_t seed, uint32_t value) {
    return seed ^ (value + 0x9e3779b9U + (seed << 6U) + (seed >> 2U));
}

uint32_t sample_seed(
    uint32_t base_seed,
    size_t input_size,
    InputDataCase input_case,
    size_t run,
    bool warmup
) {
    uint32_t seed = mix_seed(base_seed, static_cast<uint32_t>(input_size));
    seed = mix_seed(seed, static_cast<uint32_t>(input_case));
    seed = mix_seed(seed, static_cast<uint32_t>(run));
    return mix_seed(seed, warmup ? 0x5741524dU : 0x4d454153U);
}

BenchmarkStatistics make_statistics(const std::vector<uint64_t>& values) {
    BenchmarkStatistics statistics;
    statistics.sample_count = values.size();
    if (values.empty()) {
        return statistics;
    }

    statistics.minimum = get_minimum(values);
    statistics.maximum = get_maximum(values);
    statistics.mean = get_average(values);
    statistics.median = get_median(values);
    statistics.p25 = get_percentile(values, 25.0);
    statistics.p75 = get_percentile(values, 75.0);
    statistics.p90 = get_percentile(values, 90.0);
    statistics.p95 = get_percentile(values, 95.0);
    statistics.p99 = get_percentile(values, 99.0);
    statistics.standard_deviation = get_standard_deviation(values, statistics.mean);

    const double iqr = statistics.p75 - statistics.p25;
    const double lower_fence = statistics.p25 - 1.5 * iqr;
    const double upper_fence = statistics.p75 + 1.5 * iqr;
    for (uint64_t value : values) {
        if (static_cast<double>(value) < lower_fence ||
            static_cast<double>(value) > upper_fence) {
            ++statistics.outlier_count;
        }
    }
    return statistics;
}

const BenchmarkSummary* find_summary(
    const std::vector<BenchmarkSummary>& summaries,
    const std::string& key,
    InputDataCase input_case,
    size_t input_size
) {
    for (const BenchmarkSummary& summary : summaries) {
        if (summary.key == key && summary.input_case == input_case &&
            summary.input_size == input_size) {
            return &summary;
        }
    }
    return nullptr;
}

std::vector<uint64_t> find_raw_values(
    const std::vector<BenchmarkMeasurement>& measurements,
    const BenchmarkSummary& summary,
    bool time_values
) {
    std::vector<uint64_t> values;
    for (const BenchmarkMeasurement& measurement : measurements) {
        if (measurement.benchmark_key == summary.key &&
            measurement.input_case == summary.input_case &&
            measurement.input_size == summary.input_size) {
            values.push_back(time_values ? measurement.time_ns : measurement.cycles);
        }
    }
    return values;
}

size_t count_successful_runs(
    const std::vector<BenchmarkMeasurement>& measurements,
    const BenchmarkSummary& summary
) {
    size_t result = 0;
    for (const BenchmarkMeasurement& measurement : measurements) {
        if (measurement.benchmark_key == summary.key &&
            measurement.input_case == summary.input_case &&
            measurement.input_size == summary.input_size &&
            (!measurement.verification_performed || measurement.verified)) {
            ++result;
        }
    }
    return result;
}

SystemSnapshot system_snapshot() {
    SystemInfo info;
    return {
        info.operating_system(),
        info.cpu_name(),
        info.architecture(),
        info.logical_processors(),
        info.physical_cores(),
        info.compiler(),
        info.cxx_standard(),
        info.optimization()
    };
}

ComplexityResult complexity_for(
    const std::vector<BenchmarkSummary>& summaries,
    const Benchmark& benchmark,
    InputDataCase input_case,
    const BenchmarkConfig& config
) {
    std::vector<size_t> sizes;
    std::vector<double> timings;
    for (size_t size : config.input_sizes) {
        const BenchmarkSummary* summary =
            find_summary(summaries, benchmark.key, input_case, size);
        if (summary != nullptr) {
            sizes.push_back(size);
            timings.push_back(timing_statistics(*summary, config).median);
        }
    }
    ComplexityAnalyzer analyzer;
    ComplexityResult result = analyzer.analyze(sizes, timings);
    result.theoretical_complexity = benchmark.theoretical_complexity;
    return result;
}

void print_header(const SystemSnapshot& system, const BenchmarkConfig& config) {
    std::ostringstream sizes;
    for (size_t index = 0; index < config.input_sizes.size(); ++index) {
        if (index > 0) {
            sizes << ", ";
        }
        sizes << comma_number(config.input_sizes[index]);
    }

    std::cout
        << "\n╔══════════════════════════════════════════════════════════════════════╗\n"
        << "║                  CODE PERFORMANCE ANALYZER v3.0                     ║\n"
        << "║              Algorithm Benchmarking & Analysis Framework            ║\n"
        << "╚══════════════════════════════════════════════════════════════════════╝\n\n"
        << "SYSTEM INFORMATION\n" << divider << '\n'
        << std::left
        << std::setw(20) << "OS" << ": " << system.os << '\n'
        << std::setw(20) << "CPU" << ": " << system.cpu << '\n'
        << std::setw(20) << "Architecture" << ": " << system.architecture << '\n'
        << std::setw(20) << "Physical Cores" << ": " << system.physical_cores << '\n'
        << std::setw(20) << "Logical CPUs" << ": " << system.logical_cpus << '\n'
        << std::setw(20) << "Compiler" << ": " << system.compiler << '\n'
        << std::setw(20) << "C++ Standard" << ": " << system.cxx_standard << '\n'
        << std::setw(20) << "Optimization" << ": " << system.optimization << "\n\n"
        << "BENCHMARK CONFIGURATION\n" << divider << '\n'
        << std::setw(20) << "Warm-up runs" << ": " << config.warmup_runs << '\n'
        << std::setw(20) << "Measurement runs" << ": " << config.iterations << '\n'
        << std::setw(20) << "Input sizes" << ": " << sizes.str() << '\n';
    if (config.is_custom_file) {
        std::cout
            << std::setw(20) << "Input source" << ": " << "Custom File (" << config.custom_file_path << ")\n"
            << std::setw(20) << "Random seed" << ": " << "N/A (user dataset)\n";
    } else {
        std::cout
            << std::setw(20) << "Input distributions" << ": " << join_cases(config.input_cases) << '\n'
            << std::setw(20) << "Random seed" << ": " << config.random_seed << '\n';
    }
    std::cout
        << std::setw(20) << "Timer" << ": " << timer_name(config) << '\n'
        << std::setw(20) << "Memory measurement" << ": " << (config.measure_memory ? "Enabled" : "Disabled") << '\n'
        << std::setw(20) << "CPU affinity" << ": ";
    if (config.cpu_affinity >= 0) {
        std::cout << "CPU " << config.cpu_affinity;
    } else {
        std::cout << "Disabled";
    }
    std::cout << "\n\n";
}

void print_summary_table(
    const std::vector<BenchmarkSummary>& summaries,
    const std::vector<Benchmark>& benchmarks,
    const BenchmarkConfig& config
) {
    if (summaries.empty()) {
        return;
    }
    const size_t size = config.input_sizes[config.input_sizes.size() / 2];
    const InputDataCase input_case = config.input_cases.front();
    std::cout
        << divider << "\nBENCHMARK: SORTING ALGORITHMS\n" << divider << "\n\n"
        << std::left
        << std::setw(18) << "Algorithm"
        << std::setw(16) << "Input"
        << std::setw(10) << "N"
        << std::setw(14) << "Median"
        << std::setw(14) << "P95"
        << "Status\n"
        << divider << '\n';
    for (const Benchmark& benchmark : benchmarks) {
        const BenchmarkSummary* summary =
            find_summary(summaries, benchmark.key, input_case, size);
        if (summary == nullptr) {
            continue;
        }
        const BenchmarkStatistics& statistics = timing_statistics(*summary, config);
        std::cout
            << std::setw(18) << benchmark.name
            << std::setw(16) << input_data_case_name(input_case)
            << std::setw(10) << comma_number(size)
            << std::setw(14) << format_measurement(statistics.median, config)
            << std::setw(14) << format_measurement(statistics.p95, config)
            << verification_status(*summary) << '\n';
    }
    std::cout << '\n';
}

const Benchmark* preferred_benchmark(const std::vector<Benchmark>& benchmarks) {
    for (const Benchmark& benchmark : benchmarks) {
        if (benchmark.key == "quicksort") {
            return &benchmark;
        }
    }
    return benchmarks.empty() ? nullptr : &benchmarks.front();
}

void print_detailed_result(
    const std::vector<BenchmarkSummary>& summaries,
    const std::vector<BenchmarkMeasurement>& measurements,
    const std::vector<Benchmark>& benchmarks,
    const BenchmarkConfig& config
) {
    const Benchmark* benchmark = preferred_benchmark(benchmarks);
    if (benchmark == nullptr) {
        return;
    }
    const BenchmarkSummary* summary = find_summary(
        summaries, benchmark->key, config.input_cases.front(), config.input_sizes.back()
    );
    if (summary == nullptr) {
        return;
    }
    const BenchmarkStatistics& statistics = timing_statistics(*summary, config);
    const std::vector<uint64_t> values =
        find_raw_values(measurements, *summary, config.use_high_resolution_timer);
    const double cv = get_coefficient_of_variation(values, statistics.mean) * 100.0;

    std::cout
        << "DETAILED RESULT\n" << divider << "\n\n"
        << benchmark->name << '\n'
        << "Input distribution  : " << input_data_case_name(summary->input_case) << '\n'
        << "Input size          : " << comma_number(summary->input_size) << '\n'
        << "Iterations          : " << config.iterations << "\n\n"
        << "Timing\n"
        << "  Minimum           : " << format_measurement(statistics.minimum, config) << '\n'
        << "  P25               : " << format_measurement(statistics.p25, config) << '\n'
        << "  Median            : " << format_measurement(statistics.median, config) << '\n'
        << "  Mean              : " << format_measurement(statistics.mean, config) << '\n'
        << "  P75               : " << format_measurement(statistics.p75, config) << '\n'
        << "  P90               : " << format_measurement(statistics.p90, config) << '\n'
        << "  P95               : " << format_measurement(statistics.p95, config) << '\n'
        << "  P99               : " << format_measurement(statistics.p99, config) << '\n'
        << "  Maximum           : " << format_measurement(statistics.maximum, config) << '\n'
        << "  Std deviation     : " << format_measurement(statistics.standard_deviation, config) << "\n\n"
        << "Reliability\n"
        << "  Successful runs   : " << count_successful_runs(measurements, *summary)
        << " / " << statistics.sample_count << '\n'
        << "  Outliers detected : " << statistics.outlier_count << '\n'
        << "  Variation (CV)    : " << std::fixed << std::setprecision(2) << cv << "%\n"
        << "  Correctness       : " << verification_status(*summary) << "\n\n";
}

void print_memory_analysis(
    const std::vector<BenchmarkSummary>& summaries,
    const std::vector<Benchmark>& benchmarks,
    const BenchmarkConfig& config
) {
    if (!config.measure_memory) {
        std::cout
            << "MEMORY ANALYSIS\n"
            << divider << "\n\n"
            << "Disabled\n\n";
        return;
    }
    const Benchmark* benchmark = preferred_benchmark(benchmarks);
    if (benchmark == nullptr) {
        return;
    }
    const BenchmarkSummary* summary = find_summary(
        summaries, benchmark->key, config.input_cases.front(), config.input_sizes.back()
    );
    if (summary == nullptr) {
        return;
    }

    std::cout
        << "MEMORY ANALYSIS\n"
        << divider << "\n\n"
        << "Private Memory\n"
        << "  Baseline          : " << format_mb(static_cast<double>(summary->memory.baseline.private_bytes)) << '\n'
        << "  Peak observed     : " << format_mb(static_cast<double>(summary->memory.peak.private_bytes)) << '\n'
        << "  Peak increase     : " << format_mb(static_cast<double>(summary->memory.peak_private_increase)) << '\n'
        << "  Net change        : " << format_mb(static_cast<double>(summary->memory.net_private_change)) << "\n\n"
        << "Working Set\n"
        << "  Baseline          : " << format_mb(static_cast<double>(summary->memory.baseline.working_set_bytes)) << '\n'
        << "  Peak observed     : " << format_mb(static_cast<double>(summary->memory.peak.working_set_bytes)) << '\n'
        << "  Peak increase     : " << format_mb(static_cast<double>(summary->memory.peak_working_set_increase)) << '\n'
        << "  Net change        : " << format_mb(static_cast<double>(summary->memory.net_working_set_change)) << "\n\n";
}

void print_complexity_analysis(
    const std::vector<BenchmarkSummary>& summaries,
    const std::vector<Benchmark>& benchmarks,
    const BenchmarkConfig& config
) {
    std::cout
        << "COMPLEXITY ANALYSIS\n" << divider << "\n\n"
        << std::left
        << std::setw(18) << "Algorithm"
        << std::setw(20) << "Theoretical"
        << std::setw(22) << "Observed Scaling"
        << "Fit quality\n" << divider << '\n';
    for (const Benchmark& benchmark : benchmarks) {
        const ComplexityResult result =
            complexity_for(summaries, benchmark, config.input_cases.front(), config);
        std::ostringstream fit;
        fit << std::fixed << std::setprecision(2) << result.fit_quality;
        std::cout
            << std::setw(18) << benchmark.name
            << std::setw(20) << result.theoretical_complexity
            << std::setw(22) << result.observed_model
            << fit.str() << '\n';
    }
    std::cout
        << "\nObserved scaling is an experimental log-log fit; it does not prove Big-O complexity.\n"
        << "* Quick Sort theoretical complexity is average-case.\n\n";
}

void print_performance_comparison(
    const std::vector<BenchmarkSummary>& summaries,
    const std::vector<Benchmark>& benchmarks,
    const BenchmarkConfig& config
) {
    const size_t size = config.input_sizes.back();
    const InputDataCase input_case = config.input_cases.front();
    std::vector<const BenchmarkSummary*> ranking;
    for (const Benchmark& benchmark : benchmarks) {
        if (const BenchmarkSummary* summary =
                find_summary(summaries, benchmark.key, input_case, size)) {
            ranking.push_back(summary);
        }
    }
    std::sort(ranking.begin(), ranking.end(), [&config](
        const BenchmarkSummary* left,
        const BenchmarkSummary* right
    ) {
        return timing_statistics(*left, config).median <
            timing_statistics(*right, config).median;
    });

    std::cout
        << "PERFORMANCE COMPARISON\n" << divider << "\n\n"
        << "Fastest algorithm @ N = " << comma_number(size) << '\n';
    for (size_t index = 0; index < ranking.size(); ++index) {
        std::cout
            << "  " << index + 1 << ". "
            << std::left << std::setw(18) << ranking[index]->name
            << format_measurement(timing_statistics(*ranking[index], config).median, config)
            << '\n';
    }
    std::cout << '\n';
}

void print_distribution_analysis(
    const std::vector<BenchmarkSummary>& summaries,
    const std::vector<Benchmark>& benchmarks,
    const BenchmarkConfig& config
) {
    const Benchmark* benchmark = preferred_benchmark(benchmarks);
    if (benchmark == nullptr) {
        return;
    }
    const size_t size = config.input_sizes.back();
    std::cout
        << "INPUT DISTRIBUTION ANALYSIS\n" << divider << "\n\n"
        << benchmark->name << " @ N = " << comma_number(size) << "\n\n";
    for (InputDataCase input_case : config.input_cases) {
        const BenchmarkSummary* summary =
            find_summary(summaries, benchmark->key, input_case, size);
        if (summary != nullptr) {
            std::cout
                << std::left << std::setw(20) << input_data_case_name(input_case)
                << format_measurement(timing_statistics(*summary, config).median, config)
                << '\n';
        }
    }
    std::cout << '\n';
}

std::string json_escape(const std::string& value) {
    std::ostringstream output;
    for (unsigned char character : value) {
        switch (character) {
        case '"': output << "\\\""; break;
        case '\\': output << "\\\\"; break;
        case '\n': output << "\\n"; break;
        case '\r': output << "\\r"; break;
        case '\t': output << "\\t"; break;
        default:
            if (character < 0x20) {
                output << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                    << static_cast<int>(character) << std::dec << std::setfill(' ');
            } else {
                output << character;
            }
        }
    }
    return output.str();
}

std::string timestamp_for_file() {
    const std::time_t now = std::time(nullptr);
    std::tm local_time{};
    localtime_s(&local_time, &now);
    std::ostringstream output;
    output << std::put_time(&local_time, "%Y-%m-%d_%H-%M-%S");
    return output.str();
}

ReportPaths create_report_paths() {
    const std::filesystem::path directory = "results";
    std::filesystem::create_directories(directory);
    const std::string timestamp = timestamp_for_file();
    for (unsigned int suffix = 0;; ++suffix) {
        const std::string suffix_text = suffix == 0 ? "" : "_" + std::to_string(suffix);
        const std::filesystem::path base =
            directory / ("benchmark_" + timestamp + suffix_text);
        const ReportPaths paths = {base.string() + ".csv", base.string() + ".json"};
        if (!std::filesystem::exists(paths.csv) && !std::filesystem::exists(paths.json)) {
            return paths;
        }
    }
}

bool write_csv_report(
    const ReportPaths& paths,
    const std::vector<BenchmarkMeasurement>& measurements
) {
    std::ofstream report(paths.csv);
    if (!report) {
        return false;
    }
    report
        << "benchmark_key,input_data,input_size,iteration,cycles,time_ns,"
        << "private_memory_delta_bytes,working_set_delta_bytes,verification_performed,verified\n";
    for (const BenchmarkMeasurement& measurement : measurements) {
        report
            << '"' << json_escape(measurement.benchmark_key) << "\","
            << '"' << json_escape(input_data_case_name(measurement.input_case)) << "\","
            << measurement.input_size << ','
            << measurement.iteration << ','
            << measurement.cycles << ','
            << measurement.time_ns << ','
            << measurement.private_memory_delta << ','
            << measurement.working_set_delta << ','
            << (measurement.verification_performed ? "true" : "false") << ','
            << (measurement.verified ? "true" : "false") << '\n';
    }
    return static_cast<bool>(report);
}

void write_statistics_json(
    std::ostream& output,
    const BenchmarkStatistics& statistics,
    const std::string& indent
) {
    output
        << indent << "\"minimum\": " << statistics.minimum << ",\n"
        << indent << "\"maximum\": " << statistics.maximum << ",\n"
        << indent << "\"mean\": " << statistics.mean << ",\n"
        << indent << "\"median\": " << statistics.median << ",\n"
        << indent << "\"p25\": " << statistics.p25 << ",\n"
        << indent << "\"p75\": " << statistics.p75 << ",\n"
        << indent << "\"p90\": " << statistics.p90 << ",\n"
        << indent << "\"p95\": " << statistics.p95 << ",\n"
        << indent << "\"p99\": " << statistics.p99 << ",\n"
        << indent << "\"standard_deviation\": " << statistics.standard_deviation << ",\n"
        << indent << "\"sample_count\": " << statistics.sample_count << ",\n"
        << indent << "\"outlier_count\": " << statistics.outlier_count << '\n';
}

bool write_json_report(
    const ReportPaths& paths,
    const SystemSnapshot& system,
    const BenchmarkConfig& config,
    const std::vector<BenchmarkSummary>& summaries,
    const std::vector<BenchmarkMeasurement>& measurements,
    const analysis::BenchmarkRun& run_data
) {
    std::ofstream report(paths.json);
    if (!report) {
        return false;
    }
    report << std::fixed << std::setprecision(6);
    report
        << "{\n"
        << "  \"format_version\": \"4.0\",\n"
        << "  \"run_id\": \"" << json_escape(run_data.run_id) << "\",\n"
        << "  \"timestamp\": \"" << json_escape(run_data.timestamp) << "\",\n"
        << "  \"generated_at\": \"" << json_escape(run_data.timestamp) << "\",\n";

    if (config.is_custom_file && FileInputLoader::has_active_dataset()) {
        const DatasetInfo& info = FileInputLoader::get_active_dataset_info();
        report
            << "  \"input\": {\n"
            << "    \"type\": \"custom_file\",\n"
            << "    \"file\": \"" << json_escape(config.custom_file_path) << "\",\n"
            << "    \"elements\": " << info.element_count << ",\n"
            << "    \"distinct\": " << info.distinct_count << ",\n"
            << "    \"duplicates\": " << info.duplicate_count << ",\n"
            << "    \"min\": " << info.min_value << ",\n"
            << "    \"max\": " << info.max_value << ",\n"
            << "    \"order\": \"" << json_escape(info.order_description) << "\"\n"
            << "  },\n";
    } else {
        report
            << "  \"input\": {\n"
            << "    \"type\": \"generated\"\n"
            << "  },\n";
    }

    report
        << "  \"system\": {\n"
        << "    \"operating_system\": \"" << json_escape(system.os) << "\",\n"
        << "    \"cpu\": \"" << json_escape(system.cpu) << "\",\n"
        << "    \"architecture\": \"" << json_escape(system.architecture) << "\",\n"
        << "    \"physical_cores\": " << system.physical_cores << ",\n"
        << "    \"logical_processors\": " << system.logical_cpus << ",\n"
        << "    \"compiler\": \"" << json_escape(system.compiler) << "\",\n"
        << "    \"cxx_standard\": \"" << json_escape(system.cxx_standard) << "\",\n"
        << "    \"optimization\": \"" << json_escape(system.optimization) << "\"\n"
        << "  },\n"
        << "  \"configuration\": {\n"
        << "    \"warmup_runs\": " << config.warmup_runs << ",\n"
        << "    \"iterations\": " << config.iterations << ",\n"
        << "    \"random_seed\": " << config.random_seed << ",\n"
        << "    \"use_rdtsc\": " << (config.use_rdtsc ? "true" : "false") << ",\n"
        << "    \"use_high_resolution_timer\": "
        << (config.use_high_resolution_timer ? "true" : "false") << ",\n"
        << "    \"measure_memory\": "
        << (config.measure_memory ? "true" : "false") << ",\n"
        << "    \"cpu_affinity\": " << config.cpu_affinity << ",\n"
        << "    \"input_sizes\": [";
    for (size_t index = 0; index < config.input_sizes.size(); ++index) {
        report << (index == 0 ? "" : ", ") << config.input_sizes[index];
    }
    report << "],\n    \"input_cases\": [";
    for (size_t index = 0; index < config.input_cases.size(); ++index) {
        report << (index == 0 ? "" : ", ")
            << '"' << json_escape(input_data_case_name(config.input_cases[index])) << '"';
    }
    report << "]\n  },\n  \"results\": [\n";
    for (size_t index = 0; index < run_data.results.size(); ++index) {
        const auto& r = run_data.results[index];
        report
            << "    {\n"
            << "      \"algorithm\": \"" << json_escape(r.algorithm) << "\",\n"
            << "      \"algorithm_name\": \"" << json_escape(r.algorithm_name) << "\",\n"
            << "      \"input_type\": \"" << json_escape(r.input_type) << "\",\n"
            << "      \"input_size\": " << r.input_size << ",\n"
            << "      \"verified\": " << (r.verified ? "true" : "false") << ",\n"
            << "      \"time_mean_ns\": " << r.time_mean_ns << ",\n"
            << "      \"time_median_ns\": " << r.time_median_ns << ",\n"
            << "      \"time_min_ns\": " << r.time_min_ns << ",\n"
            << "      \"time_max_ns\": " << r.time_max_ns << ",\n"
            << "      \"time_stddev_ns\": " << r.time_stddev_ns << ",\n"
            << "      \"cycles_mean\": " << r.cycles_mean << ",\n"
            << "      \"cycles_median\": " << r.cycles_median << ",\n"
            << "      \"cycles_min\": " << r.cycles_min << ",\n"
            << "      \"cycles_max\": " << r.cycles_max << ",\n"
            << "      \"cycles_stddev\": " << r.cycles_stddev << ",\n"
            << "      \"memory_peak_increase_bytes\": " << r.memory_peak_increase_bytes << ",\n"
            << "      \"memory_net_change_bytes\": " << r.memory_net_change_bytes << "\n"
            << "    }" << (index + 1 == run_data.results.size() ? "" : ",") << "\n";
    }
    report << "  ],\n  \"summaries\": [\n";
    for (size_t index = 0; index < summaries.size(); ++index) {
        const BenchmarkSummary& summary = summaries[index];
        report
            << "    {\n"
            << "      \"key\": \"" << json_escape(summary.key) << "\",\n"
            << "      \"name\": \"" << json_escape(summary.name) << "\",\n"
            << "      \"input_size\": " << summary.input_size << ",\n"
            << "      \"input_case\": \"" << json_escape(input_data_case_name(summary.input_case)) << "\",\n"
            << "      \"verification_performed\": "
            << (summary.verification_performed ? "true" : "false") << ",\n"
            << "      \"verified\": " << (summary.verified ? "true" : "false") << ",\n"
            << "      \"cycles\": {\n";
        write_statistics_json(report, summary.cycles, "        ");
        report << "      },\n      \"time_ns\": {\n";
        write_statistics_json(report, summary.time, "        ");
        report << "      },\n      \"private_memory_delta_bytes\": {\n";
        write_statistics_json(report, summary.private_memory, "        ");
        report << "      },\n      \"working_set_delta_bytes\": {\n";
        write_statistics_json(report, summary.working_set, "        ");
        report << "      },\n      \"memory_analysis\": {\n";
        report << "        \"private_baseline_bytes\": " << summary.memory.baseline.private_bytes << ",\n";
        report << "        \"private_peak_bytes\": " << summary.memory.peak.private_bytes << ",\n";
        report << "        \"private_peak_increase_bytes\": " << summary.memory.peak_private_increase << ",\n";
        report << "        \"private_net_change_bytes\": " << summary.memory.net_private_change << ",\n";
        report << "        \"working_set_baseline_bytes\": " << summary.memory.baseline.working_set_bytes << ",\n";
        report << "        \"working_set_peak_bytes\": " << summary.memory.peak.working_set_bytes << ",\n";
        report << "        \"working_set_peak_increase_bytes\": " << summary.memory.peak_working_set_increase << ",\n";
        report << "        \"working_set_net_change_bytes\": " << summary.memory.net_working_set_change << "\n";
        report << "      }\n    }" << (index + 1 == summaries.size() ? "\n" : ",\n");
    }
    report << "  ],\n  \"measurements\": [\n";
    for (size_t index = 0; index < measurements.size(); ++index) {
        const BenchmarkMeasurement& measurement = measurements[index];
        report
            << "    {\"benchmark_key\": \"" << json_escape(measurement.benchmark_key)
            << "\", \"input_size\": " << measurement.input_size
            << ", \"input_case\": \"" << json_escape(input_data_case_name(measurement.input_case))
            << "\", \"iteration\": " << measurement.iteration
            << ", \"cycles\": " << measurement.cycles
            << ", \"time_ns\": " << measurement.time_ns
            << ", \"private_memory_delta_bytes\": " << measurement.private_memory_delta
            << ", \"working_set_delta_bytes\": " << measurement.working_set_delta
            << ", \"verification_performed\": "
            << (measurement.verification_performed ? "true" : "false")
            << ", \"verified\": " << (measurement.verified ? "true" : "false")
            << "}" << (index + 1 == measurements.size() ? "\n" : ",\n");
    }
    report << "  ]\n}\n";
    return static_cast<bool>(report);
}

BenchmarkSummary run_single_benchmark(
    const Benchmark& benchmark,
    size_t input_size,
    InputDataCase input_case,
    const BenchmarkConfig& config,
    uint64_t overhead,
    std::vector<BenchmarkMeasurement>& measurements
) {
    for (int run = 0; run < config.warmup_runs; ++run) {
        run_benchmark(
            benchmark.setup, benchmark.function, input_size, input_case,
            sample_seed(config.random_seed, input_size, input_case, static_cast<size_t>(run), true),
            config.use_rdtsc, config.use_high_resolution_timer, overhead,
            config.cpu_affinity, false
        );
    }

    std::vector<uint64_t> cycles;
    std::vector<uint64_t> times;
    const bool verification_performed = static_cast<bool>(benchmark.verify);
    bool verified = true;
    for (int run = 0; run < config.iterations; ++run) {
        const BenchmarkResult result = run_benchmark(
            benchmark.setup, benchmark.function, input_size, input_case,
            sample_seed(config.random_seed, input_size, input_case, static_cast<size_t>(run), false),
            config.use_rdtsc, config.use_high_resolution_timer, overhead,
            config.cpu_affinity, config.measure_memory
        );
        // Verification happens only after both timers have stopped.
        const bool measurement_verified =
            !verification_performed || benchmark.verify();
        verified = verified && measurement_verified;
        if (config.use_rdtsc) {
            cycles.push_back(result.cycles);
        }
        if (config.use_high_resolution_timer) {
            times.push_back(result.time_ns);
        }
        measurements.push_back({
            benchmark.key, input_size, input_case, static_cast<size_t>(run),
            result.cycles, result.time_ns, result.private_memory_delta,
            result.working_set_delta, verification_performed, measurement_verified,
            result.memory
        });
    }

    BenchmarkSummary summary;
    summary.key = benchmark.key;
    summary.name = benchmark.name;
    summary.input_size = input_size;
    summary.input_case = input_case;
    summary.verification_performed = verification_performed;
    summary.verified = verified;
    summary.cycles = make_statistics(cycles);
    std::vector<uint64_t> private_memory;
    std::vector<uint64_t> working_set;
    if (config.measure_memory) {
        bool memory_set = false;
        for (const BenchmarkMeasurement& measurement : measurements) {
            if (measurement.benchmark_key == benchmark.key &&
                measurement.input_case == input_case &&
                measurement.input_size == input_size) {
                private_memory.push_back(measurement.private_memory_delta);
                working_set.push_back(measurement.working_set_delta);
                if (!memory_set ||
                    measurement.memory.peak_private_increase > summary.memory.peak_private_increase) {
                    summary.memory = measurement.memory;
                    memory_set = true;
                }
            }
        }
    }
    summary.time = make_statistics(times);
    summary.private_memory = make_statistics(private_memory);
    summary.working_set = make_statistics(working_set);
    return summary;
}

bool valid_config(const BenchmarkConfig& config) {
    const unsigned int logical_cpus = CpuAffinityGuard::logical_processor_count();
    const bool affinity_valid = config.cpu_affinity < 0 ||
        (config.cpu_affinity < 64 &&
         static_cast<unsigned int>(config.cpu_affinity) < logical_cpus);

    return config.warmup_runs >= 0 && config.iterations > 0 &&
        !config.input_sizes.empty() && !config.input_cases.empty() &&
        (config.use_rdtsc || config.use_high_resolution_timer) &&
        affinity_valid &&
        std::all_of(config.input_sizes.begin(), config.input_sizes.end(),
            [] (size_t size) { return size > 0; });
}

} // namespace

BenchmarkRunner::BenchmarkRunner(BenchmarkConfig config)
    : benchmark_config(std::move(config)) {
    if (benchmark_config.use_rdtsc) {
        overhead = measure_overhead();
    }
}

void BenchmarkRunner::add(
    const std::string& key,
    const std::string& name,
    const std::string& theoretical_complexity,
    BenchmarkSetupFunction setup,
    BenchmarkFunction function,
    BenchmarkVerificationFunction verify
) {
    benchmarks.push_back({key, name, theoretical_complexity, setup, function, verify});
}

void BenchmarkRunner::set_input_sizes(const std::vector<size_t>& sizes) {
    benchmark_config.input_sizes = sizes;
}

void BenchmarkRunner::set_input_cases(const std::vector<InputDataCase>& input_cases) {
    benchmark_config.input_cases = input_cases;
}

const BenchmarkConfig& BenchmarkRunner::config() const {
    return benchmark_config;
}

const std::vector<BenchmarkSummary>& BenchmarkRunner::results() const {
    return summaries;
}

const std::vector<BenchmarkMeasurement>& BenchmarkRunner::raw_measurements() const {
    return measurements;
}

const analysis::BenchmarkRun& BenchmarkRunner::last_analysis_run() const {
    return last_run_data;
}

analysis::BenchmarkRun BenchmarkRunner::to_analysis_run() const {
    analysis::BenchmarkRun run;
    run.format_version = "4.0";
    const std::string ts = timestamp_for_file();
    run.timestamp = ts;
    run.run_id = "RUN-" + ts;

    const SystemSnapshot system = system_snapshot();
    run.system.os = system.os;
    run.system.cpu = system.cpu;
    run.system.architecture = system.architecture;
    run.system.physical_cores = system.physical_cores;
    run.system.logical_cpus = system.logical_cpus;
    run.system.compiler = system.compiler;
    run.system.cxx_standard = system.cxx_standard;
    run.system.optimization = system.optimization;

    run.configuration.warmup_runs = benchmark_config.warmup_runs;
    run.configuration.iterations = benchmark_config.iterations;
    run.configuration.random_seed = benchmark_config.random_seed;
    run.configuration.use_rdtsc = benchmark_config.use_rdtsc;
    run.configuration.use_high_resolution_timer = benchmark_config.use_high_resolution_timer;
    run.configuration.measure_memory = benchmark_config.measure_memory;
    run.configuration.cpu_affinity = benchmark_config.cpu_affinity;
    run.configuration.input_sizes = benchmark_config.input_sizes;
    for (InputDataCase input_case : benchmark_config.input_cases) {
        run.configuration.input_cases.push_back(input_data_case_name(input_case));
    }

    if (benchmark_config.is_custom_file && FileInputLoader::has_active_dataset()) {
        const DatasetInfo& info = FileInputLoader::get_active_dataset_info();
        run.input.type = "custom_file";
        run.input.file_path = benchmark_config.custom_file_path;
        run.input.element_count = info.element_count;
        run.input.distinct_count = info.distinct_count;
        run.input.duplicate_count = info.duplicate_count;
        run.input.min_value = info.min_value;
        run.input.max_value = info.max_value;
        run.input.order_description = info.order_description;
    } else {
        run.input.type = "generated";
        if (!benchmark_config.input_sizes.empty()) {
            run.input.element_count = benchmark_config.input_sizes[0];
        }
    }

    for (const BenchmarkSummary& summary : summaries) {
        analysis::BenchmarkRecord rec;
        rec.algorithm = summary.key;
        rec.algorithm_name = summary.name;
        rec.input_type = input_data_case_name(summary.input_case);
        rec.input_size = summary.input_size;
        rec.verified = summary.verified;

        rec.time_mean_ns = summary.time.mean;
        rec.time_median_ns = summary.time.median;
        rec.time_min_ns = summary.time.minimum;
        rec.time_max_ns = summary.time.maximum;
        rec.time_stddev_ns = summary.time.standard_deviation;

        rec.cycles_mean = summary.cycles.mean;
        rec.cycles_median = summary.cycles.median;
        rec.cycles_min = summary.cycles.minimum;
        rec.cycles_max = summary.cycles.maximum;
        rec.cycles_stddev = summary.cycles.standard_deviation;

        rec.memory_peak_increase_bytes = summary.memory.peak_private_increase;
        rec.memory_net_change_bytes = summary.memory.net_private_change;

        run.results.push_back(rec);
    }
    return run;
}

void BenchmarkRunner::run_experiment(const std::vector<Benchmark>& selected_benchmarks) {
    if (selected_benchmarks.empty()) {
        throw std::invalid_argument("No benchmarks are registered.");
    }
    if (!valid_config(benchmark_config)) {
        throw std::invalid_argument(
            "BenchmarkConfig is invalid: iterations/input sizes/input cases must be valid, "
            "at least one timer must be enabled, and CPU affinity must be valid."
        );
    }
    summaries.clear();
    measurements.clear();
    const auto started = std::chrono::steady_clock::now();
    const SystemSnapshot system = system_snapshot();
    print_header(system, benchmark_config);
    if (benchmark_config.use_rdtsc) {
        std::cout << "RDTSC timer overhead : " << overhead << " cycles\n\n";
    }

    for (InputDataCase input_case : benchmark_config.input_cases) {
        for (size_t input_size : benchmark_config.input_sizes) {
            for (const Benchmark& benchmark : selected_benchmarks) {
                summaries.push_back(run_single_benchmark(
                    benchmark, input_size, input_case, benchmark_config, overhead, measurements
                ));
            }
        }
    }

    print_summary_table(summaries, selected_benchmarks, benchmark_config);
    print_detailed_result(summaries, measurements, selected_benchmarks, benchmark_config);
    print_memory_analysis(summaries, selected_benchmarks, benchmark_config);
    print_complexity_analysis(summaries, selected_benchmarks, benchmark_config);
    print_performance_comparison(summaries, selected_benchmarks, benchmark_config);
    print_distribution_analysis(summaries, selected_benchmarks, benchmark_config);

    last_run_data = to_analysis_run();

    try {
        const ReportPaths paths = create_report_paths();
        const bool csv_ok = write_csv_report(paths, measurements);
        const bool json_ok = write_json_report(
            paths, system, benchmark_config, summaries, measurements, last_run_data
        );
        std::cout << "OUTPUT\n" << divider << '\n';
        if (csv_ok) {
            std::cout << "CSV report          : " << paths.csv.generic_string() << '\n';
        } else {
            std::cerr << "Error: unable to write CSV report\n";
        }
        if (json_ok) {
            std::cout << "JSON report         : " << paths.json.generic_string() << '\n';
        } else {
            std::cerr << "Error: unable to write JSON report\n";
        }

        analysis::HistoryManager history;
        const std::string history_file = history.save_run(last_run_data);
        if (!history_file.empty()) {
            std::cout << "History archive     : " << history_file << '\n';
        }
    } catch (const std::filesystem::filesystem_error& error) {
        std::cerr << "Error: unable to create results directory: " << error.what() << '\n';
    }

    const double elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - started
    ).count();
    std::cout
        << "\nBenchmark completed successfully.\n"
        << "Total execution time: " << std::fixed << std::setprecision(2)
        << elapsed << " seconds\n\n"
        << "╔══════════════════════════════════════════════════════════════════════╗\n"
        << "║                    BENCHMARK COMPLETE ✓                             ║\n"
        << "╚══════════════════════════════════════════════════════════════════════╝\n";
}

void BenchmarkRunner::run_all() {
    run_experiment(benchmarks);
}

bool BenchmarkRunner::run_selected(const std::string& key) {
    for (const Benchmark& benchmark : benchmarks) {
        if (benchmark.key == key) {
            run_experiment({benchmark});
            return true;
        }
    }
    return false;
}

void BenchmarkRunner::list_benchmarks() const {
    std::cout << "Available benchmarks:\n\n";
    for (const Benchmark& benchmark : benchmarks) {
        std::cout
            << "  --" << std::left << std::setw(18) << benchmark.key
            << benchmark.name << " (" << benchmark.theoretical_complexity << ")\n";
    }
    std::cout
        << "\nInput cases: Random, Sorted, Reverse, Nearly Sorted, Many Duplicates, All Equal\n"
        << "CSV contains raw measurements; JSON records the structured experiment.\n\n"
        << "Usage:\n"
        << "  analyzer.exe --all\n"
        << "  analyzer.exe --quick\n"
        << "  analyzer.exe --file dataset.txt\n"
        << "  analyzer.exe --file dataset.txt --quicksort\n"
        << "  analyzer.exe --quicksort --sizes 100,500,1000\n"
        << "  analyzer.exe --all --cases random,sorted,nearly-sorted\n"
        << "  analyzer.exe --all --iterations 20 --warmup 5 --no-rdtsc\n";
}
