#ifndef BENCHMARK_RUNNER_H
#define BENCHMARK_RUNNER_H

#include "Benchmark.h"

#include <vector>
#include <string>
#include <stdint.h>


struct BenchmarkSummary{

    std::string key;
    std::string name;

    double average_ticks;
    double median_ticks;

    double average_time_ns;
    double median_time_ns;
};


class BenchmarkRunner{

private:

    std::vector<Benchmark>benchmarks;

    std::vector<BenchmarkSummary>summaries;

    int warmup_runs;
    int iterations;

    uint64_t overhead;


public:

    BenchmarkRunner(
        int warmup=10,
        int iterations=100
    );


    void add(

        const std::string&key,

        const std::string&name,

        BenchmarkFunction setup,

        BenchmarkFunction function
    );


    void run_all();


    bool run_selected(
        const std::string&key
    );


    void list_benchmarks() const;
};


BenchmarkRunner&get_benchmark_runner();

#endif