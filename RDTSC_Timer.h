#ifndef RDTSC_TIMER_H
#define RDTSC_TIMER_H

#include <stdint.h>

class RDTSC_Timer{

private:

    uint64_t start_ticks;
    uint64_t end_ticks;

public:

    void start();

    void stop();

    uint64_t ticks() const;
};

#endif