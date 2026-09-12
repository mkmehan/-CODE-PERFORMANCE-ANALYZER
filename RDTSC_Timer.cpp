#include "RDTSC_Timer.h"

#include <x86intrin.h>

void RDTSC_Timer::start() {
    _mm_mfence();
    _mm_lfence();
    start_ticks_ = __rdtsc();
    _mm_lfence();
}

void RDTSC_Timer::stop() {
    unsigned int aux = 0;
    end_ticks_ = __rdtscp(&aux);
    _mm_lfence();
}

uint64_t RDTSC_Timer::elapsed() const {
    return end_ticks_ >= start_ticks_ ? end_ticks_ - start_ticks_ : 0;
}
