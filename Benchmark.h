#ifndef BENCHMARK_H
#define BENCHMARK_H

#include "MemoryMonitor.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

enum class InputDataCase {
    Random,
    Sorted,
    ReverseSorted,
    NearlySorted,
    ManyDuplicates,
    AllEqual
};

std::string input_data_case_name(InputDataCase input_case);

using BenchmarkSetupFunction =
    std::function<void(size_t, InputDataCase, uint32_t)>;
using BenchmarkFunction = std::function<void(size_t)>;
using BenchmarkVerificationFunction = std::function<bool()>;

struct Benchmark {
    std::string key;
    std::string name;
    std::string theoretical_complexity;
    BenchmarkSetupFunction setup;
    BenchmarkFunction function;
    BenchmarkVerificationFunction verify;
};

struct BenchmarkResult {
    uint64_t cycles = 0;
    uint64_t time_ns = 0;
    uint64_t private_memory_delta = 0;
    uint64_t working_set_delta = 0;
    MemoryPeak memory{};
};

struct BenchmarkStatistics {
    uint64_t minimum = 0;
    uint64_t maximum = 0;

    double mean = 0.0;
    double median = 0.0;

    double p25 = 0.0;
    double p75 = 0.0;
    double p90 = 0.0;
    double p95 = 0.0;
    double p99 = 0.0;

    double standard_deviation = 0.0;

    size_t sample_count = 0;
    size_t outlier_count = 0;
};

struct BenchmarkSummary {
    std::string key;
    std::string name;

    size_t input_size = 0;
    InputDataCase input_case = InputDataCase::Random;

    bool verification_performed = false;
    bool verified = true;

    BenchmarkStatistics cycles;
    BenchmarkStatistics time;
    BenchmarkStatistics private_memory;
    BenchmarkStatistics working_set;
    MemoryPeak memory{};
};

struct BenchmarkMeasurement {
    std::string benchmark_key;
    size_t input_size = 0;
    InputDataCase input_case = InputDataCase::Random;
    size_t iteration = 0;
    uint64_t cycles = 0;
    uint64_t time_ns = 0;
    uint64_t private_memory_delta = 0;
    uint64_t working_set_delta = 0;
    bool verification_performed = false;
    bool verified = true;
    MemoryPeak memory{};
};

uint64_t measure_overhead();

// This routine enforces the measurement order: setup, timers, algorithm,
// timers. Call verification only after this function returns.
BenchmarkResult run_benchmark(
    BenchmarkSetupFunction setup,
    BenchmarkFunction function,
    size_t input_size,
    InputDataCase input_case,
    uint32_t seed,
    bool use_rdtsc,
    bool use_high_resolution_timer,
    uint64_t overhead,
    int cpu_affinity,
    bool measure_memory
);

#endif
