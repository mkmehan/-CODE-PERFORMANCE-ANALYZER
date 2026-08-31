#include "BenchmarkRunner.h"
#include "Statistics.h"

#include <algorithm>
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


    // Default input sizes

    input_sizes={
        100,
        500,
        1000,
        2000
    };
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


void BenchmarkRunner::set_input_sizes(

    const std::vector<size_t>&sizes

){

    input_sizes=sizes;
}


// ========================================
// Run one benchmark for one input size
// ========================================

static BenchmarkSummary run_single_benchmark(

    const Benchmark&benchmark,

    size_t input_size,

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

            input_size,

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

                input_size,

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


    // =================================
    // Detailed output
    // =================================

    std::cout
        <<"----------------------------------------\n";


    std::cout
        <<"Benchmark : "
        <<benchmark.name
        <<'\n';


    std::cout
        <<"Input size: "
        <<input_size
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

        input_size,

        average_ticks,

        median_ticks,

        average_time,

        median_time

    };
}


// ========================================
// Print comparison for one input size
// ========================================

static void print_size_comparison(

    const std::vector<BenchmarkSummary>&results,

    size_t input_size

){

    if(results.empty()){

        return;
    }


    std::vector<size_t>ranking;


    for(size_t i=0;i<results.size();i++){

        ranking.push_back(i);
    }


    std::sort(

        ranking.begin(),

        ranking.end(),

        [&results](size_t a,size_t b){

            return
                results[a].average_time_ns
                <
                results[b].average_time_ns;
        }
    );


    std::cout
        <<"========================================\n";


    std::cout
        <<"         INPUT SIZE: "
        <<input_size
        <<"\n";


    std::cout
        <<"========================================\n\n";


    std::cout
        <<std::left
        <<std::setw(8)
        <<"Rank"

        <<std::setw(25)
        <<"Benchmark"

        <<std::setw(20)
        <<"Average Time"

        <<std::setw(18)
        <<"Average TSC"

        <<'\n';


    std::cout
        <<"-----------------------------------------------------------\n";


    for(size_t rank=0;rank<ranking.size();rank++){

        const BenchmarkSummary&result=
            results[ranking[rank]];


        std::ostringstream average_time;

        average_time
            <<std::fixed
            <<std::setprecision(2)
            <<result.average_time_ns
            <<" ns";


        std::ostringstream average_ticks;

        average_ticks
            <<std::fixed
            <<std::setprecision(2)
            <<result.average_ticks;


        std::cout
            <<std::left
            <<std::setw(8)
            <<rank+1

            <<std::setw(25)
            <<result.name

            <<std::setw(20)
            <<average_time.str()

            <<std::setw(18)
            <<average_ticks.str()

            <<'\n';
    }


    std::cout
        <<"-----------------------------------------------------------\n";


    const BenchmarkSummary&fastest=
        results[ranking[0]];


    std::cout
        <<"Fastest : "
        <<fastest.name
        <<'\n';


    std::cout
        <<"Time    : "
        <<std::fixed
        <<std::setprecision(2)
        <<fastest.average_time_ns
        <<" ns\n";


    std::cout
        <<"========================================\n\n";
}


// ========================================
// Global scaling comparison
// ========================================


static void print_scaling_comparison(
    const std::vector<BenchmarkSummary>&summaries,
    const std::vector<size_t>&input_sizes,
    const std::vector<Benchmark>&benchmarks
){
    if(summaries.empty()){

        return;
    }


    std::cout
        <<"========================================\n";


    std::cout
        <<"          PERFORMANCE vs INPUT SIZE\n";


    std::cout
        <<"========================================\n\n";


    std::cout
        <<std::left
        <<std::setw(25)
        <<"Benchmark";


    for(size_t size:input_sizes){

        std::ostringstream header;

        header
            <<"N="<<size;


        std::cout
            <<std::setw(16)
            <<header.str();
    }


    std::cout
        <<'\n';


    std::cout
        <<"-----------------------------------------------------------\n";


    for(const Benchmark&benchmark:benchmarks){

        std::cout
            <<std::left
            <<std::setw(25)
            <<benchmark.name;


        for(size_t input_size:input_sizes){

            double value=0.0;


            for(
                const BenchmarkSummary&summary:
                summaries
            ){

                if(

                    summary.key==benchmark.key
                    &&
                    summary.input_size==input_size

                ){

                    value=
                        summary.average_time_ns;

                    break;
                }
            }


            std::ostringstream time;

            time
                <<std::fixed
                <<std::setprecision(2)
                <<value/1000000.0
                <<" ms";


            std::cout
                <<std::setw(16)
                <<time.str();
        }


        std::cout
            <<'\n';
    }


    std::cout
        <<"===========================================================\n";
}


// ========================================
// Run all benchmarks
// ========================================


void BenchmarkRunner::run_all(){

    summaries.clear();


    double tsc_frequency=
        get_tsc_frequency();


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


    for(size_t input_size:input_sizes){

        std::vector<BenchmarkSummary>size_results;


        for(const Benchmark&benchmark:benchmarks){

            std::cout
                <<"Starting : "
                <<benchmark.name
                <<" | N="
                <<input_size
                <<'\n';


            BenchmarkSummary summary=

                run_single_benchmark(

                    benchmark,

                    input_size,

                    warmup_runs,

                    iterations,

                    overhead
                );


            summaries.push_back(
                summary
            );


            size_results.push_back(
                summary
            );


            std::cout
                <<"Finished : "
                <<benchmark.name
                <<" | N="
                <<input_size
                <<"\n\n";
        }


        print_size_comparison(
            size_results,
            input_size
        );
    }


    print_scaling_comparison(

        summaries,

        input_sizes,

        benchmarks
    );
}



// ========================================
// Run selected benchmark
// ========================================

bool BenchmarkRunner::run_selected(

    const std::string&key

){

    double tsc_frequency=
        get_tsc_frequency();


    for(const Benchmark&benchmark:benchmarks){

        if(benchmark.key==key){

            std::cout
                <<"========================================\n";


            std::cout
                <<"       CODE PERFORMANCE ANALYZER V2\n";


            std::cout
                <<"========================================\n\n";


            std::cout
                <<"TSC frequency : "
                <<std::fixed
                <<std::setprecision(3)
                <<tsc_frequency
                <<" GHz\n\n";


            for(size_t input_size:input_sizes){

                std::cout
                    <<"Running "
                    <<benchmark.name
                    <<" with N="
                    <<input_size
                    <<"\n";


                run_single_benchmark(

                    benchmark,

                    input_size,

                    warmup_runs,

                    iterations,

                    overhead
                );


                std::cout
                    <<'\n';
            }


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
        <<"Available benchmarks:\n\n";


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
        <<"\nInput sizes:\n";

    for(size_t size:input_sizes){

        std::cout
            <<"  N="
            <<size
            <<'\n';
    }


    std::cout
        <<"\n  --all"
        <<"         Run all benchmarks\n";


    std::cout
        <<"  --help"
        <<"        Show available benchmarks\n";
}