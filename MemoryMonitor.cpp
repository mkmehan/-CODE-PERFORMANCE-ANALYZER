#include "MemoryMonitor.h"

#include <windows.h>
#include <psapi.h>

MemorySampler::MemorySampler() = default;

MemorySampler::~MemorySampler() {
    if (active_) {
        stop();
    }
}

void MemorySampler::start() {
    if (active_) {
        return;
    }
    baseline_ = MemoryMonitor::snapshot();
    peak_private_.store(baseline_.private_bytes, std::memory_order_relaxed);
    peak_working_set_.store(baseline_.working_set_bytes, std::memory_order_relaxed);
    stop_flag_.store(false, std::memory_order_relaxed);
    active_ = true;

    worker_ = std::thread([this]() {
        // Explicitly assign full process affinity to the sampler thread
        // so it does not inherit any single-core affinity from the benchmark thread.
        DWORD_PTR process_mask = 0;
        DWORD_PTR system_mask = 0;
        if (GetProcessAffinityMask(GetCurrentProcess(), &process_mask, &system_mask) && process_mask != 0) {
            SetThreadAffinityMask(GetCurrentThread(), process_mask);
        }

        while (!stop_flag_.load(std::memory_order_relaxed)) {
            const MemorySnapshot current = MemoryMonitor::snapshot();

            uint64_t prev_priv = peak_private_.load(std::memory_order_relaxed);
            while (current.private_bytes > prev_priv &&
                   !peak_private_.compare_exchange_weak(prev_priv, current.private_bytes, std::memory_order_relaxed)) {
            }

            uint64_t prev_ws = peak_working_set_.load(std::memory_order_relaxed);
            while (current.working_set_bytes > prev_ws &&
                   !peak_working_set_.compare_exchange_weak(prev_ws, current.working_set_bytes, std::memory_order_relaxed)) {
            }

            // Short wait/yield
            std::this_thread::yield();
        }
    });
}

MemoryPeak MemorySampler::stop() {
    if (!active_) {
        return {};
    }

    stop_flag_.store(true, std::memory_order_relaxed);
    if (worker_.joinable()) {
        worker_.join();
    }
    active_ = false;

    const MemorySnapshot final_snap = MemoryMonitor::snapshot();

    uint64_t max_priv = peak_private_.load(std::memory_order_relaxed);
    if (final_snap.private_bytes > max_priv) {
        max_priv = final_snap.private_bytes;
    }

    uint64_t max_ws = peak_working_set_.load(std::memory_order_relaxed);
    if (final_snap.working_set_bytes > max_ws) {
        max_ws = final_snap.working_set_bytes;
    }

    MemoryPeak result;
    result.baseline = baseline_;
    result.peak.private_bytes = max_priv;
    result.peak.working_set_bytes = max_ws;
    result.final_snapshot = final_snap;

    result.peak_private_increase =
        max_priv > baseline_.private_bytes ? max_priv - baseline_.private_bytes : 0;

    result.peak_working_set_increase =
        max_ws > baseline_.working_set_bytes ? max_ws - baseline_.working_set_bytes : 0;

    result.net_private_change =
        MemoryMonitor::signed_delta(baseline_.private_bytes, final_snap.private_bytes);

    result.net_working_set_change =
        MemoryMonitor::signed_delta(baseline_.working_set_bytes, final_snap.working_set_bytes);

    return result;
}

int64_t MemoryMonitor::signed_delta(uint64_t before, uint64_t after) {
    if (after >= before) {
        const uint64_t delta = after - before;
        return delta > static_cast<uint64_t>(INT64_MAX) ? INT64_MAX : static_cast<int64_t>(delta);
    }
    const uint64_t delta = before - after;
    return delta > static_cast<uint64_t>(INT64_MAX) ? INT64_MIN : -static_cast<int64_t>(delta);
}

MemorySnapshot MemoryMonitor::snapshot() {
    PROCESS_MEMORY_COUNTERS_EX counters{};
    counters.cb = sizeof(counters);

    if (!GetProcessMemoryInfo(
            GetCurrentProcess(),
            reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
            sizeof(counters))) {
        return {};
    }

    return {
        static_cast<uint64_t>(counters.WorkingSetSize),
        static_cast<uint64_t>(counters.PrivateUsage)
    };
}

uint64_t MemoryMonitor::private_delta(
    const MemorySnapshot& before,
    const MemorySnapshot& after
) {
    const int64_t delta = signed_delta(before.private_bytes, after.private_bytes);
    return delta < 0 ? 0 : static_cast<uint64_t>(delta);
}

uint64_t MemoryMonitor::working_set_delta(
    const MemorySnapshot& before,
    const MemorySnapshot& after
) {
    const int64_t delta = signed_delta(before.working_set_bytes, after.working_set_bytes);
    return delta < 0 ? 0 : static_cast<uint64_t>(delta);
}

