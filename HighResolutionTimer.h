#ifndef HIGH_RESOLUTION_TIMER_H
#define HIGH_RESOLUTION_TIMER_H

#include "Timer.h"

#include <windows.h>

class HighResolutionTimer : public Timer {
private:
    LARGE_INTEGER start_time{};
    LARGE_INTEGER end_time{};
    LARGE_INTEGER frequency{};

public:
    HighResolutionTimer();

    void start() override;
    void stop() override;
    uint64_t elapsed() const override;
};

#endif
