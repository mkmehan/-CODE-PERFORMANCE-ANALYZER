#include "../BenchmarkRunner.h"
#include "RegisterBenchmarks.h"


void register_bubble_sort(
    BenchmarkRunner&runner
);

void register_insertion_sort(
    BenchmarkRunner&runner
);

void register_selection_sort(
    BenchmarkRunner&runner
);

void register_merge_sort(
    BenchmarkRunner&runner
);

void register_quick_sort(
    BenchmarkRunner&runner
);

void register_sum(
    BenchmarkRunner&runner
);


void register_all_benchmarks(
    BenchmarkRunner&runner
){

    register_bubble_sort(runner);

    register_insertion_sort(runner);

    register_selection_sort(runner);

    register_merge_sort(runner);

    register_quick_sort(runner);

    register_sum(runner);
}