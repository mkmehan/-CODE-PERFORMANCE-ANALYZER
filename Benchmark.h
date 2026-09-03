#ifndef BENCHMARK_H
#define BENCHMARK_H

#include <string>
#include <functional>
#include <cstddef>
#include <cstdint>

using BenchmarkFunction=std::function<void(size_t)>;

struct Benchmark{

    std::string key;
    std::string name;

    BenchmarkFunction setup;
    BenchmarkFunction function;
};

struct BenchmarkResult{

    uint64_t cycles;
    uint64_t time_ns;
};

struct BenchmarkSummary{

    std::string key;
    std::string name;

    size_t input_size;

    double average_ticks;
    double median_ticks;

    double average_time_ns;
    double median_time_ns;
};

uint64_t measure_overhead();

double get_tsc_frequency();

BenchmarkResult run_benchmark(

    BenchmarkFunction setup,
    BenchmarkFunction function,

    size_t input_size,

    uint64_t overhead

);

#endif
