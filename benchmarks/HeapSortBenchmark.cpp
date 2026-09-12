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
    heap_sort(data);
}
} // namespace

void register_heap_sort(BenchmarkRunner& runner) {
    runner.add(
        "heap", "Heap Sort", "O(n log n)", setup, run,
        [] { return std::is_sorted(data.begin(), data.end()); }
    );
}
