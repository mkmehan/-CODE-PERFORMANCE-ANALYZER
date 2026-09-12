#ifndef TIMER_H
#define TIMER_H

#include <cstdint>

// A timer measures only the code placed between start() and stop().
class Timer {
public:
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual uint64_t elapsed() const = 0;
    virtual ~Timer() = default;
};

#endif
