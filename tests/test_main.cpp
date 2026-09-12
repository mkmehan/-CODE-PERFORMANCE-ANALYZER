#include "../benchmarks/SortingAlgorithms.h"
#include "../benchmarks/SortingData.h"
#include "../ComplexityAnalyzer.h"
#include "../MemoryMonitor.h"
#include "../CpuAffinity.h"
#include "../Statistics.h"
#include "../FileInputLoader.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
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

    std::cout << "\n========================================\n";
    if (failures == 0) {
        std::cout << "RESULT: ALL TESTS PASSED\n";
    } else {
        std::cout << "RESULT: " << failures << " TEST(S) FAILED\n";
    }
    std::cout << "========================================\n";
    return failures == 0 ? 0 : 1;
}
