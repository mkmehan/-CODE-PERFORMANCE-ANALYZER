#include "RDTSC_Timer.h"

#include <x86intrin.h>

void RDTSC_Timer::start(){

    _mm_mfence();
    _mm_lfence();

    start_ticks=__rdtsc();

    _mm_lfence();
}


void RDTSC_Timer::stop(){

    unsigned int aux;

    end_ticks=__rdtscp(&aux);

    _mm_lfence();
    _mm_mfence();
}


uint64_t RDTSC_Timer::ticks() const{

    return end_ticks-start_ticks;
}