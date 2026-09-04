#include "Benchmark.h"
#include "RDTSC_Timer.h"

#include <windows.h>
#include <intrin.h>


uint64_t measure_overhead(){

    const int OVERHEAD_RUNS=1000;

    uint64_t minimum=UINT64_MAX;


    for(int i=0;i<OVERHEAD_RUNS;i++){

        RDTSC_Timer timer;

        timer.start();

        timer.stop();


        uint64_t ticks=
            timer.ticks();


        if(ticks<minimum){

            minimum=ticks;
        }
    }


    return minimum;
}


double get_tsc_frequency(){

    LARGE_INTEGER frequency;

    LARGE_INTEGER start_time;
    LARGE_INTEGER end_time;


    QueryPerformanceFrequency(
        &frequency
    );


    QueryPerformanceCounter(
        &start_time
    );


    uint64_t start_tsc=
        __rdtsc();


    Sleep(200);


    uint64_t end_tsc=
        __rdtsc();


    QueryPerformanceCounter(
        &end_time
    );


    double elapsed_time=
        (double)(
            end_time.QuadPart-
            start_time.QuadPart
        )
        /frequency.QuadPart;


    double tsc_frequency=
        (end_tsc-start_tsc)
        /elapsed_time;


    return
        tsc_frequency/
        1000000000.0;
}


BenchmarkResult run_benchmark(

    BenchmarkSetupFunction setup,
    BenchmarkFunction function,

    size_t input_size,

    InputDataCase input_case,

    uint64_t overhead

){

    LARGE_INTEGER frequency;

    LARGE_INTEGER start_time;
    LARGE_INTEGER end_time;


    QueryPerformanceFrequency(
        &frequency
    );


    // -------------------------------
    // Prepare the input
    // -------------------------------

    setup(
        input_size,
        input_case
    );


    // -------------------------------
    // Start timing
    // -------------------------------

    QueryPerformanceCounter(
        &start_time
    );


    RDTSC_Timer timer;

    timer.start();


    // -------------------------------
    // Run actual code
    // -------------------------------

    function(input_size);


    // -------------------------------
    // Stop timing
    // -------------------------------

    timer.stop();


    QueryPerformanceCounter(
        &end_time
    );


    // -------------------------------
    // TSC result
    // -------------------------------

    uint64_t measured_ticks=
        timer.ticks();


    uint64_t actual_ticks=0;


    if(measured_ticks>overhead){

        actual_ticks=
            measured_ticks-overhead;
    }


    // -------------------------------
    // High-resolution time
    // -------------------------------

    uint64_t counter_difference=
        end_time.QuadPart-
        start_time.QuadPart;


    uint64_t time_ns=
        (uint64_t)(
            (double)counter_difference
            *1000000000.0
            /frequency.QuadPart
        );


    return{

        actual_ticks,
        time_ns
    };
}
