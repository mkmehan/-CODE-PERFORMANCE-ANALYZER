#ifndef BENCHMARK_H
#define BENCHMARK_H

#include <string>
#include <functional>
#include <cstddef>
#include <cstdint>

enum class InputDataCase{

    Random,
    Sorted,
    ReverseSorted
};


using BenchmarkSetupFunction=
    std::function<void(size_t,InputDataCase)>;

using BenchmarkFunction=std::function<void(size_t)>;

using BenchmarkVerificationFunction=std::function<bool()>;

struct Benchmark{

    std::string key;
    std::string name;

    BenchmarkSetupFunction setup;
    BenchmarkFunction function;
    BenchmarkVerificationFunction verify;
};

struct BenchmarkResult{

    uint64_t cycles;
    uint64_t time_ns;
};

struct BenchmarkSummary{

    std::string key;
    std::string name;

    size_t input_size;
    InputDataCase input_case;
    bool verification_performed;
    bool verified;

    double average_ticks;
    double median_ticks;

    double average_time_ns;
    double median_time_ns;
};

uint64_t measure_overhead();

double get_tsc_frequency();

BenchmarkResult run_benchmark(

    BenchmarkSetupFunction setup,
    BenchmarkFunction function,

    size_t input_size,

    InputDataCase input_case,

    uint64_t overhead

);

#endif
