#ifndef BENCHMARK_RUNNER_H
#define BENCHMARK_RUNNER_H

#include "Benchmark.h"

#include <vector>
#include <string>
#include <cstdint>


class BenchmarkRunner{

private:

    std::vector<Benchmark>benchmarks;

    std::vector<BenchmarkSummary>summaries;

    std::vector<size_t>input_sizes;

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

        BenchmarkSetupFunction setup,

        BenchmarkFunction function,

        BenchmarkVerificationFunction verify={}

    );


    void add(

        const std::string&key,

        const std::string&name,

        BenchmarkFunction setup,

        BenchmarkFunction function

    );


    void set_input_sizes(

        const std::vector<size_t>&sizes

    );


    void run_all();


    bool run_selected(

        const std::string&key

    );


    void list_benchmarks() const;

};

#endif
