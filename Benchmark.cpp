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

        uint64_t ticks=timer.ticks();

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


    uint64_t start_tsc=__rdtsc();


    Sleep(200);


    uint64_t end_tsc=__rdtsc();


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


    return tsc_frequency/1000000000.0;
}


BenchmarkResult run_benchmark(

    BenchmarkFunction setup,
    BenchmarkFunction function,
    uint64_t overhead

){

    LARGE_INTEGER frequency;

    LARGE_INTEGER start_time;
    LARGE_INTEGER end_time;


    QueryPerformanceFrequency(
        &frequency
    );


    // Prepare data BEFORE measurement

    setup();


    // Start high-resolution timer

    QueryPerformanceCounter(
        &start_time
    );


    // Start RDTSC

    RDTSC_Timer timer;

    timer.start();


    // Run actual benchmark

    function();


    // Stop RDTSC

    timer.stop();


    // Stop high-resolution timer

    QueryPerformanceCounter(
        &end_time
    );


    uint64_t measured_ticks=
        timer.ticks();


    uint64_t actual_ticks=0;


    if(measured_ticks>overhead){

        actual_ticks=
            measured_ticks-overhead;
    }


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
    //
}