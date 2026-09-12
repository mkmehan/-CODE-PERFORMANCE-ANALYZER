#include "../BenchmarkRunner.h"
#include "SortingAlgorithms.h"
#include "SortingData.h"

#include <algorithm>
#include <vector>

namespace {
std::vector<int> data;
std::vector<int> temporary;

void setup(size_t size, InputDataCase input_case, uint32_t seed) {
    data = get_sorting_input(size, input_case, seed);
    temporary.resize(data.size());
}

void run(size_t) {
    merge_sort(data, temporary);
}
} // namespace

void register_merge_sort(BenchmarkRunner& runner) {
    runner.add(
        "merge", "Merge Sort", "O(n log n)", setup, run,
        [] { return std::is_sorted(data.begin(), data.end()); }
    );
}

