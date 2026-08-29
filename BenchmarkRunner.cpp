#include "BenchmarkRunner.h"
#include "Statistics.h"

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
BenchmarkRunner::BenchmarkRunner(
    int warmup,
    int iterations
){

    warmup_runs=warmup;

    this->iterations=iterations;

    overhead=measure_overhead();
}


void BenchmarkRunner::add(

    const std::string&key,

    const std::string&name,

    BenchmarkFunction setup,

    BenchmarkFunction function

){

    benchmarks.push_back({

        key,
        name,
        setup,
        function
    });
}


BenchmarkRunner&get_benchmark_runner(){

    static BenchmarkRunner runner(
        10,
        100
    );

    return runner;
}


// ========================================
// Run one benchmark
// ========================================

static BenchmarkSummary run_single_benchmark(

    const Benchmark&benchmark,

    int warmup_runs,

    int iterations,

    uint64_t overhead

){

    std::vector<uint64_t>tick_results;

    std::vector<uint64_t>time_results;


    // --------------------------------
    // Warm-up
    // --------------------------------

    for(int i=0;i<warmup_runs;i++){

        run_benchmark(

            benchmark.setup,

            benchmark.function,

            overhead
        );
    }


    // --------------------------------
    // Actual measurements
    // --------------------------------

    for(int i=0;i<iterations;i++){

        BenchmarkResult result=

            run_benchmark(

                benchmark.setup,

                benchmark.function,

                overhead
            );


        tick_results.push_back(
            result.cycles
        );


        time_results.push_back(
            result.time_ns
        );
    }


    // --------------------------------
    // Statistics
    // --------------------------------

    double average_ticks=
        get_average(tick_results);


    double median_ticks=
        get_median(tick_results);


    double average_time=
        get_average(time_results);


    double median_time=
        get_median(time_results);


    double stddev_ticks=
        get_standard_deviation(
            tick_results,
            average_ticks
        );


    double stddev_time=
        get_standard_deviation(
            time_results,
            average_time
        );


    uint64_t minimum_ticks=
        get_minimum(tick_results);


    uint64_t maximum_ticks=
        get_maximum(tick_results);


    uint64_t minimum_time=
        get_minimum(time_results);


    uint64_t maximum_time=
        get_maximum(time_results);


    // --------------------------------
    // Detailed report
    // --------------------------------

    std::cout
        <<"----------------------------------------\n";


    std::cout
        <<"Algorithm : "
        <<benchmark.name
        <<'\n';


    std::cout
        <<"Command   : --"
        <<benchmark.key
        <<'\n';


    std::cout
        <<"----------------------------------------\n";


    std::cout
        <<"Warm-up runs  : "
        <<warmup_runs
        <<'\n';


    std::cout
        <<"Measured runs : "
        <<iterations
        <<"\n\n";


    std::cout
        <<"RDTSC\n";


    std::cout
        <<"Minimum ticks : "
        <<minimum_ticks
        <<'\n';


    std::cout
        <<"Maximum ticks : "
        <<maximum_ticks
        <<'\n';


    std::cout
        <<"Average ticks : "
        <<std::fixed
        <<std::setprecision(2)
        <<average_ticks
        <<'\n';


    std::cout
        <<"Median ticks  : "
        <<median_ticks
        <<'\n';


    std::cout
        <<"Std deviation : "
        <<stddev_ticks
        <<" ticks\n\n";


    std::cout
        <<"High Resolution Timer\n";


    std::cout
        <<"Minimum time  : "
        <<minimum_time
        <<" ns\n";


    std::cout
        <<"Maximum time  : "
        <<maximum_time
        <<" ns\n";


    std::cout
        <<"Average time  : "
        <<average_time
        <<" ns\n";


    std::cout
        <<"Median time   : "
        <<median_time
        <<" ns\n";


    std::cout
        <<"Std deviation : "
        <<stddev_time
        <<" ns\n\n";


    return{

        benchmark.key,
        benchmark.name,

        average_ticks,
        median_ticks,

        average_time,
        median_time
    };
}


// ========================================
// Header
// ========================================

static void print_header(

    uint64_t overhead,

    double tsc_frequency

){

    std::cout
        <<"========================================\n";


    std::cout
        <<"       CODE PERFORMANCE ANALYZER V2\n";


    std::cout
        <<"========================================\n\n";


    std::cout
        <<"Timer overhead : "
        <<overhead
        <<" TSC ticks\n";


    std::cout
        <<"TSC frequency  : "
        <<std::fixed
        <<std::setprecision(3)
        <<tsc_frequency
        <<" GHz\n\n";
}


// ========================================
// Print comparison
// ========================================

static void print_comparison(

    const std::vector<BenchmarkSummary>&summaries

){

    if(summaries.empty()){

        return;
    }


    // --------------------------------
    // Sort indices by average time
    // --------------------------------

    std::vector<size_t>ranking;


    for(size_t i=0;i<summaries.size();i++){

        ranking.push_back(i);
    }


    std::sort(

        ranking.begin(),

        ranking.end(),

        [&summaries](size_t a,size_t b){

            return
                summaries[a].average_time_ns
                <
                summaries[b].average_time_ns;
        }
    );


    // --------------------------------
    // Comparison output
    // --------------------------------

    std::cout
        <<"========================================\n";


    std::cout
        <<"           SORTING COMPARISON\n";


    std::cout
        <<"========================================\n\n";


    std::cout
        <<std::left
        <<std::setw(8)
        <<"Rank"

        <<std::setw(25)
        <<"Algorithm"

        <<std::setw(18)
        <<"Avg Time"

        <<std::setw(18)
        <<"Avg TSC"

        <<'\n';


    std::cout
        <<"-----------------------------------------------------------\n";


    for(size_t rank=0;rank<ranking.size();rank++){

        const BenchmarkSummary&summary=
            summaries[ranking[rank]];


        std::ostringstream average_time;


        average_time
            <<std::fixed
            <<std::setprecision(2)
            <<summary.average_time_ns
            <<" ns";


        std::ostringstream average_ticks;


        average_ticks
            <<std::fixed
            <<std::setprecision(2)
            <<summary.average_ticks;


        std::cout
            <<std::left
            <<std::setw(8)
            <<rank+1

            <<std::setw(25)
            <<summary.name

            <<std::setw(18)
            <<average_time.str()

            <<std::setw(18)
            <<average_ticks.str()

            <<'\n';
    }


    std::cout
        <<"-----------------------------------------------------------\n";


    // --------------------------------
    // Fastest
    // --------------------------------

    const BenchmarkSummary&fastest=
        summaries[ranking[0]];


    std::cout
        <<"Fastest algorithm : "
        <<fastest.name
        <<'\n';


    std::cout
        <<"Average time      : "
        <<fastest.average_time_ns
        <<" ns\n";


    // --------------------------------
    // Compare every algorithm to fastest
    // --------------------------------

    std::cout
        <<"\nSpeed relative to fastest:\n";


    for(size_t rank=0;rank<ranking.size();rank++){

        const BenchmarkSummary&summary=
            summaries[ranking[rank]];


        double factor=
            summary.average_time_ns
            /fastest.average_time_ns;


        std::cout
            <<"  "
            <<std::left
            <<std::setw(25)
            <<summary.name

            <<std::fixed
            <<std::setprecision(2)
            <<factor
            <<"x\n";
    }


    std::cout
        <<"\n========================================\n";
}


// ========================================
// Run all
// ========================================

    void BenchmarkRunner::run_all(){

    summaries.clear();

    double tsc_frequency=
        get_tsc_frequency();

    print_header(
        overhead,
        tsc_frequency
    );

    std::cout
        <<"Number of benchmarks: "
        <<benchmarks.size()
        <<"\n\n";

    for(size_t i=0;i<benchmarks.size();i++){

        std::cout
            <<"Starting benchmark "
            <<i+1
            <<" : "
            <<benchmarks[i].name
            <<"\n";

        BenchmarkSummary summary=
            run_single_benchmark(
                benchmarks[i],
                warmup_runs,
                iterations,
                overhead
            );

        summaries.push_back(summary);

        std::cout
            <<"Finished benchmark "
            <<i+1
            <<"\n\n";
    }

    std::cout
        <<"Finished ALL benchmarks\n\n";

    print_comparison(summaries);
}

  
    

// ========================================
// Run selected
// ========================================

bool BenchmarkRunner::run_selected(

    const std::string&key

){

    summaries.clear();


    double tsc_frequency=
        get_tsc_frequency();


    for(const Benchmark&benchmark:benchmarks){

        if(benchmark.key==key){

            print_header(

                overhead,

                tsc_frequency
            );


            run_single_benchmark(

                benchmark,

                warmup_runs,

                iterations,

                overhead
            );


            return true;
        }
    }


    return false;
}


// ========================================
// List benchmarks
// ========================================

void BenchmarkRunner::list_benchmarks() const{

    std::cout
        <<"Available sorting benchmarks:\n\n";


    for(const Benchmark&benchmark:benchmarks){

        std::cout
            <<"  --"
            <<std::left
            <<std::setw(12)
            <<benchmark.key

            <<benchmark.name
            <<'\n';
    }


    std::cout
        <<"\n  --all"
        <<"         Run all sorting benchmarks\n";


    std::cout
        <<"  --help"
        <<"        Show available benchmarks\n";
}
