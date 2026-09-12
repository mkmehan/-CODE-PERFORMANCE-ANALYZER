#ifndef BENCHMARK_CONFIG_H
#define BENCHMARK_CONFIG_H

#include "Benchmark.h"

#include <cstddef>
#include <cstdint>
#include <vector>

struct BenchmarkConfig {
    int warmup_runs = 20;
    int iterations = 100;

    std::vector<size_t> input_sizes = {100, 500, 1000, 5000, 10000};
    std::vector<InputDataCase> input_cases = {
        InputDataCase::Random,
        InputDataCase::Sorted,
        InputDataCase::ReverseSorted,
        InputDataCase::NearlySorted
    };

    uint32_t random_seed = 12345;

    bool use_rdtsc = true;
    bool use_high_resolution_timer = true;

    // -1 disables thread affinity; otherwise pin benchmark thread to logical CPU index.
    int cpu_affinity = -1;

    bool measure_memory = true;
};

#endif
