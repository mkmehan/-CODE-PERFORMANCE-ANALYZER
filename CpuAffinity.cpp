#include "CpuAffinity.h"

#include <windows.h>

CpuAffinityGuard::CpuAffinityGuard(int cpu_index) {
    if (cpu_index < 0 || cpu_index >= 64) {
        return;
    }

    const WORD bit = static_cast<WORD>(cpu_index);
    const DWORD_PTR mask = (static_cast<DWORD_PTR>(1) << bit);
    const DWORD_PTR previous = SetThreadAffinityMask(GetCurrentThread(), mask);

    if (previous != 0) {
        previous_mask_ = static_cast<uintptr_t>(previous);
        active_ = true;
    }
}

CpuAffinityGuard::~CpuAffinityGuard() {
    if (active_) {
        SetThreadAffinityMask(GetCurrentThread(), static_cast<DWORD_PTR>(previous_mask_));
    }
}

bool CpuAffinityGuard::active() const {
    return active_;
}

unsigned int CpuAffinityGuard::logical_processor_count() {
    SYSTEM_INFO info{};
    GetNativeSystemInfo(&info);
    return info.dwNumberOfProcessors;
}
