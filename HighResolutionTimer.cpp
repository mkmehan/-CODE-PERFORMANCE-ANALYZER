#include "HighResolutionTimer.h"

HighResolutionTimer::HighResolutionTimer() {
    QueryPerformanceFrequency(&frequency);
}

void HighResolutionTimer::start() {
    QueryPerformanceCounter(&start_time);
}

void HighResolutionTimer::stop() {
    QueryPerformanceCounter(&end_time);
}

uint64_t HighResolutionTimer::elapsed() const {
    if (frequency.QuadPart <= 0 || end_time.QuadPart < start_time.QuadPart) {
        return 0;
    }

    const long double nanoseconds =
        static_cast<long double>(end_time.QuadPart - start_time.QuadPart) *
        1000000000.0L / static_cast<long double>(frequency.QuadPart);

    return static_cast<uint64_t>(nanoseconds);
}
