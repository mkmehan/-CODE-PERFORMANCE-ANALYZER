#ifndef MEMORY_MONITOR_H
#define MEMORY_MONITOR_H

#include <atomic>
#include <cstdint>
#include <thread>

struct MemorySnapshot {
    uint64_t working_set_bytes = 0;
    uint64_t private_bytes = 0;
};

// Records peak observed process memory using repeated sampling
struct MemoryPeak {
    MemorySnapshot baseline{};
    MemorySnapshot peak{};
    MemorySnapshot final_snapshot{};

    uint64_t peak_private_increase = 0;
    uint64_t peak_working_set_increase = 0;
    int64_t net_private_change = 0;
    int64_t net_working_set_change = 0;
};

class MemorySampler {
public:
    MemorySampler();
    ~MemorySampler();

    MemorySampler(const MemorySampler&) = delete;
    MemorySampler& operator=(const MemorySampler&) = delete;

    void start();
    MemoryPeak stop();

private:
    MemorySnapshot baseline_{};
    std::atomic<bool> stop_flag_{false};
    std::atomic<uint64_t> peak_private_{0};
    std::atomic<uint64_t> peak_working_set_{0};
    std::thread worker_{};
    bool active_{false};
};

class MemoryMonitor {
public:
    static MemorySnapshot snapshot();

    static int64_t signed_delta(
        uint64_t before,
        uint64_t after
    );

    static uint64_t private_delta(
        const MemorySnapshot& before,
        const MemorySnapshot& after
    );

    static uint64_t working_set_delta(
        const MemorySnapshot& before,
        const MemorySnapshot& after
    );

    // Track peak observed process memory using repeated sampling while callback executes
    template <typename Function>
    static MemoryPeak measure_peak(Function&& function) {
        MemorySampler sampler;
        sampler.start();
        try {
            function();
        } catch (...) {
            sampler.stop();
            throw;
        }
        return sampler.stop();
    }
};

#endif