#include "../benchmarks/SortingAlgorithms.h"
#include "../benchmarks/SortingData.h"
#include "../ComplexityAnalyzer.h"
#include "../MemoryMonitor.h"
#include "../CpuAffinity.h"
#include "../Statistics.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
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

    std::cout << "\n========================================\n";
    if (failures == 0) {
        std::cout << "RESULT: ALL TESTS PASSED\n";
    } else {
        std::cout << "RESULT: " << failures << " TEST(S) FAILED\n";
    }
    std::cout << "========================================\n";
    return failures == 0 ? 0 : 1;
}
