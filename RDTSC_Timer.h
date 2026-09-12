#ifndef RDTSC_TIMER_H
#define RDTSC_TIMER_H

#include "Timer.h"

#include <cstdint>

class RDTSC_Timer : public Timer {
private:
    uint64_t start_ticks_ = 0;
    uint64_t end_ticks_ = 0;

public:
    void start() override;
    void stop() override;
    uint64_t elapsed() const override;
};

#endif
