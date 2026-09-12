#include "../BenchmarkRunner.h"
#include "SortingAlgorithms.h"
#include "SortingData.h"

#include <algorithm>
#include <vector>

namespace {
std::vector<int> data;

void setup(size_t size, InputDataCase input_case, uint32_t seed) {
    data = get_sorting_input(size, input_case, seed);
}

void run(size_t) {
    insertion_sort(data);
}
} // namespace

void register_insertion_sort(BenchmarkRunner& runner) {
    runner.add(
        "insertion", "Insertion Sort", "O(n^2)", setup, run,
        [] { return std::is_sorted(data.begin(), data.end()); }
    );
}

