#include "../BenchmarkRunner.h"
#include "SortingData.h"

#include <algorithm>
#include <vector>

namespace {
std::vector<int> data;

void setup(size_t size, InputDataCase input_case, uint32_t seed) {
    data = get_sorting_input(size, input_case, seed);
}

void run_sort(size_t) {
    std::sort(data.begin(), data.end());
}

void run_stable_sort(size_t) {
    std::stable_sort(data.begin(), data.end());
}
} // namespace

void register_std_sort(BenchmarkRunner& runner) {
    runner.add(
        "std-sort", "std::sort", "O(n log n)", setup, run_sort,
        [] { return std::is_sorted(data.begin(), data.end()); }
    );
}

void register_std_stable_sort(BenchmarkRunner& runner) {
    runner.add(
        "std-stable-sort", "std::stable_sort", "O(n log n)", setup, run_stable_sort,
        [] { return std::is_sorted(data.begin(), data.end()); }
    );
}
