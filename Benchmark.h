#ifndef BENCHMARK_H
#define BENCHMARK_H

#include <functional>
#include <string>
#include <stdint.h>

using BenchmarkFunction=std::function<void()>;


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


uint64_t measure_overhead();

double get_tsc_frequency();


BenchmarkResult run_benchmark(

    BenchmarkFunction setup,
    BenchmarkFunction function,
    uint64_t overhead
);

#endif