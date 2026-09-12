#ifndef CPU_AFFINITY_H
#define CPU_AFFINITY_H

#include <cstdint>

class CpuAffinityGuard {
private:
    uintptr_t previous_mask_ = 0;
    bool active_ = false;

public:
    // cpu_index < 0 disables affinity. This implementation targets logical
    // processors addressable by the current Windows processor group via the
    // legacy affinity mask API (0-63). On systems with >64 logical processors,
    // use an index in the current group or leave affinity disabled.
    explicit CpuAffinityGuard(int cpu_index);
    ~CpuAffinityGuard();

    CpuAffinityGuard(const CpuAffinityGuard&) = delete;
    CpuAffinityGuard& operator=(const CpuAffinityGuard&) = delete;

    bool active() const;
    static unsigned int logical_processor_count();
};

#endif
