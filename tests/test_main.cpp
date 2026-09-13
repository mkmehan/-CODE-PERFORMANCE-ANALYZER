#include "../benchmarks/SortingAlgorithms.h"
#include "../benchmarks/SortingData.h"
#include "../ComplexityAnalyzer.h"
#include "../MemoryMonitor.h"
#include "../CpuAffinity.h"
#include "../Statistics.h"
#include "../FileInputLoader.h"
#include "../analysis/ResultModel.h"
#include "../analysis/BenchmarkRun.h"
#include "../analysis/ReportLoader.h"
#include "../analysis/HistoryManager.h"
#include "../analysis/ComparisonAnalyzer.h"
#include "../analysis/RegressionAnalyzer.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace {
int failures = 0;

void expect(bool condition, const std::string& name) {
    if (condition) {
        std::cout << "  PASS  " << name << '\n';
    } else {
        std::cout << "  FAIL  " << name << '\n';
        ++failures;
    }
}

template <typename SortFunction>
void test_sort(const std::string& name, SortFunction sort_function) {
    const std::vector<std::vector<int>> cases = {
        {}, {1}, {2, 1}, {1, 2, 3}, {3, 2, 1},
        {5, 1, 5, 2, 5, 2, 9, 0}, {7, 7, 7, 7}
    };

    bool ok = true;
    for (std::vector<int> values : cases) {
        sort_function(values);
        if (!std::is_sorted(values.begin(), values.end())) {
            ok = false;
            break;
        }
    }
    expect(ok, name + " edge-case correctness");
}

void run_sorting_tests() {
    std::cout << "\n[SORTING TESTS]\n";
    test_sort("Bubble Sort", [](auto& v) { bubble_sort(v); });
    test_sort("Insertion Sort", [](auto& v) { insertion_sort(v); });
    test_sort("Selection Sort", [](auto& v) { selection_sort(v); });
    test_sort("Quick Sort", [](auto& v) { quick_sort(v); });
    test_sort("Heap Sort", [](auto& v) { heap_sort(v); });
    test_sort("std::sort", [](auto& v) { std::sort(v.begin(), v.end()); });
    test_sort("std::stable_sort", [](auto& v) { std::stable_sort(v.begin(), v.end()); });

    {
        std::vector<int> values = {8, 2, 7, 3, 6, 1, 5, 4};
        std::vector<int> temporary(values.size());
        merge_sort(values, temporary);
        expect(std::is_sorted(values.begin(), values.end()), "Merge Sort edge-case correctness");
    }
}

void run_input_tests() {
    std::cout << "\n[INPUT GENERATION TESTS]\n";
    const auto a = get_sorting_input(100, InputDataCase::Random, 12345);
    const auto b = get_sorting_input(100, InputDataCase::Random, 12345);
    const auto c = get_sorting_input(100, InputDataCase::Random, 54321);
    expect(a == b, "Random input is reproducible with same seed");
    expect(a != c, "Different seeds normally produce different random input");

    const auto sorted = get_sorting_input(100, InputDataCase::Sorted, 1);
    expect(std::is_sorted(sorted.begin(), sorted.end()), "Sorted distribution is ordered");

    const auto reverse = get_sorting_input(100, InputDataCase::ReverseSorted, 1);
    expect(std::is_sorted(reverse.rbegin(), reverse.rend()), "Reverse distribution is descending");

    const auto equal = get_sorting_input(100, InputDataCase::AllEqual, 1);
    expect(std::all_of(equal.begin(), equal.end(), [&](int x) { return x == equal.front(); }),
           "All-equal distribution contains one value");
}

void run_statistics_tests() {
    std::cout << "\n[STATISTICS TESTS]\n";
    const std::vector<uint64_t> values = {10, 20, 30, 40, 50};
    expect(get_minimum(values) == 10, "Minimum");
    expect(get_maximum(values) == 50, "Maximum");
    expect(std::abs(get_average(values) - 30.0) < 1e-12, "Average");
    expect(std::abs(get_median(values) - 30.0) < 1e-12, "Median");
    expect(get_percentile(values, 50.0) == 30.0, "P50");
    expect(get_percentile(values, 95.0) >= get_percentile(values, 50.0), "Percentile monotonicity");
    expect(get_standard_deviation(values, 30.0) > 0.0, "Standard deviation");
    expect(get_coefficient_of_variation(values, 30.0) > 0.0, "Coefficient of variation");
}


void run_resource_tests() {
    std::cout << "\n[RESOURCE TESTS]\n";

    const MemorySnapshot snapshot = MemoryMonitor::snapshot();
    expect(snapshot.private_bytes > 0, "Memory monitor private bytes is readable");
    expect(snapshot.working_set_bytes > 0, "Memory monitor working set is readable");

    const MemoryPeak peak = MemoryMonitor::measure_peak([]() {
        std::vector<char> buffer(10 * 1024 * 1024, 1);
        volatile char val = buffer[5 * 1024 * 1024];
        (void)val;
    });
    expect(peak.baseline.private_bytes > 0, "Memory peak baseline is readable");
    expect(peak.peak.private_bytes >= peak.baseline.private_bytes, "Memory peak >= baseline");
    expect(peak.peak_private_increase > 0, "Memory peak observes allocation increase");

    const unsigned int logical = CpuAffinityGuard::logical_processor_count();
    expect(logical > 0, "Logical CPU count is available");
    if (logical > 0 && logical <= 64) {
        CpuAffinityGuard affinity(0);
        expect(affinity.active(), "CPU affinity can pin to CPU 0");
    }
}

void run_complexity_tests() {
    std::cout << "\n[COMPLEXITY TESTS]\n";
    ComplexityAnalyzer analyzer;

    const auto quadratic = analyzer.analyze(
        {100, 200, 400, 800}, {1.0, 4.0, 16.0, 64.0});
    expect(quadratic.sample_count == 4, "Quadratic sample count");
    expect(std::abs(quadratic.exponent - 2.0) < 0.05, "Quadratic exponent");

    const auto linear = analyzer.analyze(
        {100, 200, 400, 800}, {1.0, 2.0, 4.0, 8.0});
    expect(std::abs(linear.exponent - 1.0) < 0.05, "Linear exponent");

    const auto insufficient = analyzer.analyze({100, 200}, {1.0, 2.0});
    expect(insufficient.observed_model == "Insufficient data", "Insufficient data handling");
}

void run_file_input_tests() {
    std::cout << "\n[FILE INPUT & VALIDATION TESTS]\n";

    // Test 1: Valid whitespace & newline separated dataset
    const std::string valid_file = "test_unit_valid.txt";
    {
        std::ofstream out(valid_file);
        out << "1 1 1\n2 2\n3\n4\n";
    }

    const auto valid_res = FileInputLoader::load_and_validate(valid_file);
    expect(valid_res.valid, "Valid file loads successfully");
    expect(valid_res.info.element_count == 7, "Correct element count (7)");
    expect(valid_res.info.distinct_count == 4, "Correct distinct count (4)");
    expect(valid_res.info.duplicate_count == 3, "Duplicate count = N - distinct (3)");
    expect(valid_res.info.min_value == 1, "Correct min value (1)");
    expect(valid_res.info.max_value == 4, "Correct max value (4)");
    expect(valid_res.info.is_sorted, "Sorted order detected");
    expect(valid_res.data == std::vector<int>({1, 1, 1, 2, 2, 3, 4}), "Data vector parsed correctly");

    // Test 2: In-memory cache & setup isolation (zero file I/O during sorting)
    FileInputLoader::set_active_dataset(valid_res.data, valid_res.info, valid_file);
    expect(FileInputLoader::has_active_dataset(), "Active dataset cache registered");
    std::vector<int> retrieved = get_sorting_input(0, InputDataCase::CustomFile, 0);
    expect(retrieved == valid_res.data, "get_sorting_input returns custom data");
    quick_sort(retrieved);
    expect(std::is_sorted(retrieved.begin(), retrieved.end()), "Custom data sorts correctly");
    // Verify cached master copy was not mutated by the sort
    std::vector<int> fresh_copy = get_sorting_input(0, InputDataCase::CustomFile, 0);
    expect(fresh_copy == valid_res.data, "Setup gets fresh unmutated copy from cache");
    FileInputLoader::clear_active_dataset();
    expect(!FileInputLoader::has_active_dataset(), "Active dataset cache cleared");

    // Test 3: Non-existent file rejection
    const auto missing_res = FileInputLoader::load_and_validate("non_existent_file_xyz.txt");
    expect(!missing_res.valid, "Non-existent file rejected");
    expect(missing_res.error_line == 0, "Missing file reports line 0");

    // Test 4: Empty file rejection
    const std::string empty_file = "test_unit_empty.txt";
    {
        std::ofstream out(empty_file);
        out << "   \n  \t  \n";
    }
    const auto empty_res = FileInputLoader::load_and_validate(empty_file);
    expect(!empty_res.valid, "Empty file rejected");

    // Test 5: Invalid non-integer token with line number
    const std::string invalid_file = "test_unit_invalid.txt";
    {
        std::ofstream out(invalid_file);
        out << "10\n20\nfoo\n40\n";
    }
    const auto invalid_res = FileInputLoader::load_and_validate(invalid_file);
    expect(!invalid_res.valid, "Invalid integer token rejected");
    expect(invalid_res.error_line == 3, "Correct error line detected (line 3)");
    expect(invalid_res.found == "\"foo\"", "Offending token captured");

    // Test 6: Reverse sorted order detection
    const std::string reverse_file = "test_unit_reverse.txt";
    {
        std::ofstream out(reverse_file);
        out << "100 80 50 10 -5";
    }
    const auto rev_res = FileInputLoader::load_and_validate(reverse_file);
    expect(rev_res.valid, "Reverse file loaded");
    expect(rev_res.info.is_reverse_sorted, "Reverse sorted order detected");
    expect(!rev_res.info.is_sorted, "Reverse file is not ascending sorted");

    // Clean up temporary test files
    std::remove(valid_file.c_str());
    std::remove(empty_file.c_str());
    std::remove(invalid_file.c_str());
    std::remove(reverse_file.c_str());
}

void run_result_model_tests() {
    std::cout << "\n[RESULT MODEL TESTS]\n";
    analysis::BenchmarkRecord record;
    expect(record.algorithm.empty(), "Default record has empty algorithm");
    expect(record.input_size == 0, "Default record input_size is 0");
    expect(!record.verified, "Default record verified is false");
    expect(record.memory_peak_increase_bytes == 0, "Default peak memory is 0");
    expect(record.memory_net_change_bytes == 0, "Default net memory change is 0");

    // Negative net memory change support (signed int64_t)
    record.algorithm = "quicksort";
    record.algorithm_name = "Quick Sort";
    record.input_type = "Random";
    record.input_size = 1000;
    record.verified = true;
    record.time_mean_ns = 12500.0;
    record.time_median_ns = 12000.0;
    record.cycles_mean = 35000.0;
    record.memory_peak_increase_bytes = 1048576;
    record.memory_net_change_bytes = -524288; // -0.5 MB

    expect(record.memory_net_change_bytes < 0, "Net memory change correctly represents negative value");
    expect(record.memory_net_change_bytes == -524288, "Net memory change stores exact signed value");

    // BenchmarkRun container
    analysis::BenchmarkRun run;
    expect(run.format_version == "4.0", "BenchmarkRun format_version is 4.0");
    run.run_id = "RUN-20260913-024701";
    run.timestamp = "2026-09-13_02-47-01";
    run.results.push_back(record);

    expect(run.results.size() == 1, "BenchmarkRun holds results vector");
    expect(run.results[0].algorithm_name == "Quick Sort", "BenchmarkRun preserves record details");
}

void run_report_loader_tests() {
    std::cout << "\n[REPORT LOADER TESTS]\n";

    // 1. Valid JSON string
    const std::string valid_json = R"({
  "format_version": "4.0",
  "run_id": "RUN-20260913-024701",
  "timestamp": "2026-09-13_02-47-01",
  "system": {
    "os": "Windows 11",
    "cpu": "AMD Ryzen 7",
    "architecture": "x86_64",
    "physical_cores": 8,
    "logical_processors": 16,
    "compiler": "GCC 13.2.0",
    "cxx_standard": "201703L",
    "optimization": "O2"
  },
  "configuration": {
    "warmup_runs": 3,
    "iterations": 10,
    "random_seed": 1337,
    "use_rdtsc": true,
    "use_high_resolution_timer": true,
    "measure_memory": true,
    "cpu_affinity": 0,
    "input_sizes": [1000],
    "input_cases": ["Random"]
  },
  "input": {
    "type": "custom_file",
    "file_path": "dataset.txt",
    "element_count": 1000,
    "distinct_count": 500,
    "duplicate_count": 500,
    "min_value": 1,
    "max_value": 9999,
    "order_description": "Unsorted"
  },
  "results": [
    {
      "algorithm": "quicksort",
      "algorithm_name": "Quick Sort",
      "input_type": "Custom File",
      "input_size": 1000,
      "verified": true,
      "time_mean_ns": 12500.0,
      "time_median_ns": 12000.0,
      "time_min_ns": 10000.0,
      "time_max_ns": 15000.0,
      "time_stddev_ns": 1000.0,
      "cycles_mean": 35000.0,
      "cycles_median": 34000.0,
      "cycles_min": 30000.0,
      "cycles_max": 42000.0,
      "cycles_stddev": 2500.0,
      "memory_peak_increase_bytes": 1048576,
      "memory_net_change_bytes": -524288
    }
  ]
})";

    const auto valid_res = analysis::ReportLoader::load_from_json_string(valid_json);
    expect(valid_res.success, "Valid JSON loads successfully");
    expect(valid_res.run.format_version == "4.0", "Correct format version (4.0)");
    expect(valid_res.run.run_id == "RUN-20260913-024701", "Run ID parsed correctly");
    expect(valid_res.run.system.os == "Windows 11", "System OS parsed correctly");
    expect(valid_res.run.system.logical_cpus == 16, "Logical processors parsed correctly");
    expect(valid_res.run.input.type == "custom_file", "Input type parsed correctly");
    expect(valid_res.run.input.duplicate_count == 500, "Duplicate count parsed correctly");
    expect(valid_res.run.results.size() == 1, "Results size is 1");
    expect(valid_res.run.results[0].algorithm == "quicksort", "Algorithm key matches");
    expect(valid_res.run.results[0].memory_net_change_bytes == -524288, "Negative net memory parsed accurately");

    // 2. Malformed JSON syntax
    const std::string malformed_json = "{ format_version: 4.0, broken }";
    const auto malformed_res = analysis::ReportLoader::load_from_json_string(malformed_json);
    expect(!malformed_res.success, "Malformed JSON syntax rejected");

    // 3. Incompatible or missing format_version
    const std::string bad_version_json = R"({
  "format_version": "3.0",
  "run_id": "RUN-123",
  "system": {},
  "configuration": {},
  "input": {},
  "results": []
})";
    const auto bad_version_res = analysis::ReportLoader::load_from_json_string(bad_version_json);
    expect(!bad_version_res.success, "Incompatible format_version (3.0) rejected");

    // 4. Missing required section (missing results)
    const std::string missing_results_json = R"({
  "format_version": "4.0",
  "run_id": "RUN-123",
  "system": {},
  "configuration": {},
  "input": {}
})";
    const auto missing_results_res = analysis::ReportLoader::load_from_json_string(missing_results_json);
    expect(!missing_results_res.success, "Missing results array rejected");

    // 5. Wrong type (results is not an array)
    const std::string wrong_type_json = R"({
  "format_version": "4.0",
  "run_id": "RUN-123",
  "system": {},
  "configuration": {},
  "input": {},
  "results": "not an array"
})";
    const auto wrong_type_res = analysis::ReportLoader::load_from_json_string(wrong_type_json);
    expect(!wrong_type_res.success, "Wrong field type (results as string) rejected");

    // 6. Round trip test (Serialize to JSON string -> Deserialize -> Compare)
    const std::string serialized = analysis::ReportLoader::to_json_string(valid_res.run);
    const auto roundtrip_res = analysis::ReportLoader::load_from_json_string(serialized);
    expect(roundtrip_res.success, "Serialized JSON loads successfully");
    expect(roundtrip_res.run.run_id == valid_res.run.run_id, "Roundtrip preserves run_id");
    expect(roundtrip_res.run.system.compiler == valid_res.run.system.compiler, "Roundtrip preserves system info");
    expect(roundtrip_res.run.results.size() == 1, "Roundtrip preserves results size");
    expect(roundtrip_res.run.results[0].time_mean_ns == valid_res.run.results[0].time_mean_ns, "Roundtrip preserves timing");
    expect(roundtrip_res.run.results[0].memory_net_change_bytes == -524288, "Roundtrip preserves signed memory");

    // 7. Disk file save & load roundtrip
    const std::string test_run_file = "test_run_temp.json";
    const bool save_ok = analysis::ReportLoader::save_to_json_file(valid_res.run, test_run_file);
    expect(save_ok, "ReportLoader successfully saves run to disk");
    const auto file_load_res = analysis::ReportLoader::load_from_json_file(test_run_file);
    expect(file_load_res.success, "ReportLoader successfully loads saved file from disk");
    expect(file_load_res.run.run_id == valid_res.run.run_id, "File roundtrip preserves run_id");
    std::remove(test_run_file.c_str());
}

void run_history_tests() {
    std::cout << "\n[HISTORY TESTS]\n";
    const std::string test_dir = "test_history_sandbox";
    try { std::filesystem::remove_all(test_dir); } catch (...) {}

    analysis::HistoryManager history(test_dir);

    // Initial state: empty
    expect(history.list_runs().empty(), "Initial history list is empty");
    expect(!history.get_latest_run().has_value(), "No latest run in empty history");

    // 1. Save Run 1
    analysis::BenchmarkRun run1;
    run1.format_version = "4.0";
    run1.run_id = "RUN-20260913-010000";
    run1.timestamp = "2026-09-13_01-00-00";
    run1.input.type = "generated";
    run1.input.element_count = 100;
    analysis::BenchmarkRecord rec1;
    rec1.algorithm = "quicksort";
    rec1.algorithm_name = "Quick Sort";
    rec1.input_type = "Random";
    rec1.input_size = 100;
    rec1.time_mean_ns = 5000.0;
    run1.results.push_back(rec1);

    const std::string path1 = history.save_run(run1);
    expect(!path1.empty(), "Run 1 saved successfully");
    expect(std::filesystem::exists(path1), "Run 1 file created on disk");

    // 2. Save Run 2 (newer timestamp)
    analysis::BenchmarkRun run2;
    run2.format_version = "4.0";
    run2.run_id = "RUN-20260913-020000";
    run2.timestamp = "2026-09-13_02-00-00";
    run2.input.type = "custom_file";
    run2.input.file_path = "dataset.txt";
    run2.input.element_count = 500;
    analysis::BenchmarkRecord rec2;
    rec2.algorithm = "mergesort";
    rec2.algorithm_name = "Merge Sort";
    rec2.input_type = "Custom File";
    rec2.input_size = 500;
    rec2.time_mean_ns = 8000.0;
    run2.results.push_back(rec2);

    const std::string path2 = history.save_run(run2);
    expect(!path2.empty(), "Run 2 saved successfully");

    // 3. List runs (verify count and sorting: newest first)
    const auto runs = history.list_runs();
    expect(runs.size() == 2, "History lists exactly 2 runs");
    if (runs.size() == 2) {
        expect(runs[0].run_id == "RUN-20260913-020000", "Newest run is listed first");
        expect(runs[1].run_id == "RUN-20260913-010000", "Older run is listed second");
        expect(runs[0].input_type == "custom_file", "Metadata preserves input type");
    }

    // 4. Load run by ID
    const auto loaded1 = history.load_run("RUN-20260913-010000");
    expect(loaded1.success, "Load run by ID succeeds");
    expect(loaded1.run.run_id == "RUN-20260913-010000", "Loaded run matches requested ID");
    expect(loaded1.run.results.size() == 1, "Loaded run contains benchmark results");

    // 5. Get latest run
    const auto latest = history.get_latest_run();
    expect(latest.has_value(), "get_latest_run returns a run");
    if (latest.has_value()) {
        expect(latest->run_id == "RUN-20260913-020000", "Latest run is Run 2");
    }

    // 6. Get latest run excluding run2
    const auto prev_latest = history.get_latest_run("RUN-20260913-020000");
    expect(prev_latest.has_value(), "get_latest_run with exclusion returns older run");
    if (prev_latest.has_value()) {
        expect(prev_latest->run_id == "RUN-20260913-010000", "Exclusion correctly finds previous run");
    }

    // 7. Invalid run ID handling
    const auto invalid_load = history.load_run("NON_EXISTENT_RUN_ID");
    expect(!invalid_load.success, "Invalid run ID load rejected");

    // Clean up sandbox
    try { std::filesystem::remove_all(test_dir); } catch (...) {}
}

void run_comparison_tests() {
    std::cout << "\n[COMPARISON TESTS]\n";

    // Prepare 4 algorithm records on identical input conditions
    analysis::BenchmarkRecord r_std;
    r_std.algorithm = "stdsort";
    r_std.algorithm_name = "std::sort";
    r_std.input_type = "Random";
    r_std.input_size = 10000;
    r_std.time_mean_ns = 710000.0; // 710 µs

    analysis::BenchmarkRecord r_quick;
    r_quick.algorithm = "quicksort";
    r_quick.algorithm_name = "Quick Sort";
    r_quick.input_type = "Random";
    r_quick.input_size = 10000;
    r_quick.time_mean_ns = 1240000.0; // 1240 µs

    analysis::BenchmarkRecord r_merge;
    r_merge.algorithm = "mergesort";
    r_merge.algorithm_name = "Merge Sort";
    r_merge.input_type = "Random";
    r_merge.input_size = 10000;
    r_merge.time_mean_ns = 1810000.0; // 1810 µs

    analysis::BenchmarkRecord r_heap;
    r_heap.algorithm = "heapsort";
    r_heap.algorithm_name = "Heap Sort";
    r_heap.input_type = "Random";
    r_heap.input_size = 10000;
    r_heap.time_mean_ns = 2100000.0; // 2100 µs

    std::vector<analysis::BenchmarkRecord> records = { r_merge, r_std, r_heap, r_quick };

    // 1. Compare records
    const auto report = analysis::ComparisonAnalyzer::compare_records(
        records, "Random", "", 10000, "RUN-TEST-001"
    );

    expect(report.valid, "Comparison report is valid");
    expect(!report.groups.empty(), "Report contains at least 1 group");

    if (!report.groups.empty()) {
        const auto& group = report.groups.front();

        // 2. Check fastest and slowest identification
        expect(group.fastest_algorithm == "std::sort", "Fastest algorithm correctly identified as std::sort");
        expect(group.slowest_algorithm == "Heap Sort", "Slowest algorithm correctly identified as Heap Sort");

        // 3. Check ranking order (1: std::sort, 2: Quick Sort, 3: Merge Sort, 4: Heap Sort)
        expect(group.rankings.size() == 4, "4 algorithms ranked");
        if (group.rankings.size() == 4) {
            expect(group.rankings[0].rank == 1 && group.rankings[0].algorithm_name == "std::sort", "Rank 1: std::sort");
            expect(group.rankings[1].rank == 2 && group.rankings[1].algorithm_name == "Quick Sort", "Rank 2: Quick Sort");
            expect(group.rankings[2].rank == 3 && group.rankings[2].algorithm_name == "Merge Sort", "Rank 3: Merge Sort");
            expect(group.rankings[3].rank == 4 && group.rankings[3].algorithm_name == "Heap Sort", "Rank 4: Heap Sort");

            // 4. Check speedup calculations:
            // std::sort speedup vs slowest: 2100000 / 710000 = ~2.9577x
            const double expected_speedup = 2100000.0 / 710000.0;
            expect(std::abs(group.rankings[0].speedup_vs_slowest - expected_speedup) < 0.01, "Speedup factor for fastest matches ~2.96x");
            expect(std::abs(group.rankings[3].speedup_vs_slowest - 1.0) < 0.001, "Speedup factor for slowest is 1.00x");

            // 5. Check percentage faster than slowest:
            // ((2100000 - 710000) / 2100000) * 100% = ~66.19%
            const double expected_pct = ((2100000.0 - 710000.0) / 2100000.0) * 100.0;
            expect(std::abs(group.rankings[0].percentage_faster_than_slowest - expected_pct) < 0.01, "Percentage faster than slowest computed accurately");
            expect(std::abs(group.rankings[3].percentage_faster_than_slowest - 0.0) < 0.001, "Percentage faster for slowest is 0.0%");
        }
    }

    // 6. Incompatible dataset comparison rejected
    analysis::BenchmarkRecord r_incompatible = r_quick;
    r_incompatible.input_size = 50000; // Differing input size!

    const auto bad_report = analysis::ComparisonAnalyzer::compare_records(
        { r_std, r_incompatible }, "Random", "", 10000
    );
    expect(!bad_report.valid, "Incompatible input sizes rejected");
    expect(bad_report.error_message.find("differ") != std::string::npos, "Error message specifies differing conditions");

    // Differing input type rejected
    analysis::BenchmarkRecord r_diff_type = r_quick;
    r_diff_type.input_type = "Sorted";
    const auto bad_type_report = analysis::ComparisonAnalyzer::compare_records(
        { r_std, r_diff_type }, "Random", "", 10000
    );
    expect(!bad_type_report.valid, "Incompatible input distributions rejected");

    // 7. Full BenchmarkRun comparison
    analysis::BenchmarkRun run;
    run.run_id = "RUN-TEST-002";
    run.results = records;
    const auto run_report = analysis::ComparisonAnalyzer::compare_run(run);
    expect(run_report.valid, "Full BenchmarkRun comparison succeeds");
    expect(run_report.groups.size() == 1, "Single condition group produced for homogeneous run");
}

void run_regression_tests() {
    std::cout << "\n[REGRESSION TESTS]\n";

    // Setup Baseline Run
    analysis::BenchmarkRun baseline_run;
    baseline_run.run_id = "RUN-BASE-001";
    baseline_run.input.type = "generated";
    baseline_run.input.element_count = 1000;

    analysis::BenchmarkRecord base_rec;
    base_rec.algorithm = "stdsort";
    base_rec.algorithm_name = "std::sort";
    base_rec.input_type = "Random";
    base_rec.input_size = 1000;
    base_rec.time_mean_ns = 1000.0;
    baseline_run.results.push_back(base_rec);

    // 1. Improved test (time decreased from 1000 -> 900 ns, -10%)
    {
        analysis::BenchmarkRun curr_run = baseline_run;
        curr_run.run_id = "RUN-IMPROVED-001";
        curr_run.results[0].time_mean_ns = 900.0;

        const auto rep = analysis::RegressionAnalyzer::compare_runs(curr_run, baseline_run, 5.0);
        expect(rep.valid, "Improved test report is valid");
        expect(rep.improved_count == 1, "Improved count is 1");
        expect(rep.records[0].status == analysis::RegressionStatus::Improved, "Status correctly classified as IMPROVED");
        expect(std::abs(rep.records[0].delta_percentage - (-10.0)) < 0.001, "Delta is exactly -10.0%");
    }

    // 2. Stable test (time changed from 1000 -> 1020 ns, +2%)
    {
        analysis::BenchmarkRun curr_run = baseline_run;
        curr_run.run_id = "RUN-STABLE-001";
        curr_run.results[0].time_mean_ns = 1020.0;

        const auto rep = analysis::RegressionAnalyzer::compare_runs(curr_run, baseline_run, 5.0);
        expect(rep.valid, "Stable test report is valid");
        expect(rep.stable_count == 1, "Stable count is 1");
        expect(rep.records[0].status == analysis::RegressionStatus::Stable, "Status correctly classified as STABLE");
        expect(!rep.has_regressions, "No regressions reported");
    }

    // 3. Regression test (time increased from 1000 -> 1150 ns, +15%)
    {
        analysis::BenchmarkRun curr_run = baseline_run;
        curr_run.run_id = "RUN-REGRESSION-001";
        curr_run.results[0].time_mean_ns = 1150.0;

        const auto rep = analysis::RegressionAnalyzer::compare_runs(curr_run, baseline_run, 5.0);
        expect(rep.valid, "Regression test report is valid");
        expect(rep.regressed_count == 1, "Regressed count is 1");
        expect(rep.records[0].status == analysis::RegressionStatus::Regressed, "Status correctly classified as REGRESSION");
        expect(rep.has_regressions, "Report flags has_regressions");
    }

    // 4. Boundary test: Exactly -5.0%
    {
        analysis::BenchmarkRun curr_run = baseline_run;
        curr_run.run_id = "RUN-BOUNDARY-MINUS5";
        curr_run.results[0].time_mean_ns = 950.0; // exactly -5.0%

        const auto rep = analysis::RegressionAnalyzer::compare_runs(curr_run, baseline_run, 5.0);
        expect(rep.records[0].status == analysis::RegressionStatus::Stable, "Exactly -5.0% delta classified as STABLE");
    }

    // 5. Boundary test: Exactly +5.0%
    {
        analysis::BenchmarkRun curr_run = baseline_run;
        curr_run.run_id = "RUN-BOUNDARY-PLUS5";
        curr_run.results[0].time_mean_ns = 1050.0; // exactly +5.0%

        const auto rep = analysis::RegressionAnalyzer::compare_runs(curr_run, baseline_run, 5.0);
        expect(rep.records[0].status == analysis::RegressionStatus::Stable, "Exactly +5.0% delta classified as STABLE");
        expect(!rep.has_regressions, "Exactly +5.0% is not flagged as regression");
    }

    // 6. Incompatible baseline rejected
    {
        analysis::BenchmarkRun incompatible_run;
        incompatible_run.run_id = "RUN-INCOMPATIBLE";
        incompatible_run.input.type = "custom_file";
        incompatible_run.input.file_path = "different_data.txt";
        analysis::BenchmarkRecord diff_rec;
        diff_rec.algorithm = "quicksort";
        diff_rec.input_type = "Custom File";
        diff_rec.input_size = 99999;
        incompatible_run.results.push_back(diff_rec);

        expect(!analysis::RegressionAnalyzer::are_runs_compatible(baseline_run, incompatible_run),
               "Incompatible runs detected by are_runs_compatible");

        const auto rep = analysis::RegressionAnalyzer::compare_runs(baseline_run, incompatible_run, 5.0);
        expect(!rep.valid, "Incompatible baseline rejected by compare_runs");
    }
}
}

int main() {
    std::cout << "========================================\n";
    std::cout << "CODE PERFORMANCE ANALYZER TEST SUITE\n";
    std::cout << "========================================\n";

    run_sorting_tests();
    run_input_tests();
    run_statistics_tests();
    run_resource_tests();
    run_complexity_tests();
    run_file_input_tests();
    run_result_model_tests();
    run_report_loader_tests();
    run_history_tests();
    run_comparison_tests();
    run_regression_tests();

    std::cout << "\n========================================\n";
    if (failures == 0) {
        std::cout << "RESULT: ALL TESTS PASSED\n";
    } else {
        std::cout << "RESULT: " << failures << " TEST(S) FAILED\n";
    }
    std::cout << "========================================\n";
    return failures == 0 ? 0 : 1;
}
