#include "Benchmark.h"

#include "HighResolutionTimer.h"
#include "RDTSC_Timer.h"
#include "CpuAffinity.h"
#include "MemoryMonitor.h"

#include <algorithm>
#include <limits>

std::string input_data_case_name(InputDataCase input_case) {
    switch (input_case) {
    case InputDataCase::Random: return "Random";
    case InputDataCase::Sorted: return "Sorted";
    case InputDataCase::ReverseSorted: return "Reverse";
    case InputDataCase::NearlySorted: return "Nearly Sorted";
    case InputDataCase::ManyDuplicates: return "Many Duplicates";
    case InputDataCase::AllEqual: return "All Equal";
    }
    return "Unknown";
}

uint64_t measure_overhead() {
    constexpr int overhead_runs = 1000;
    uint64_t minimum = std::numeric_limits<uint64_t>::max();

    for (int run = 0; run < overhead_runs; ++run) {
        RDTSC_Timer timer;
        timer.start();
        timer.stop();
        minimum = std::min(minimum, timer.elapsed());
    }

    return minimum == std::numeric_limits<uint64_t>::max() ? 0 : minimum;
}

BenchmarkResult run_benchmark(
    BenchmarkSetupFunction setup,
    BenchmarkFunction function,
    size_t input_size,
    InputDataCase input_case,
    uint32_t seed,
    bool use_rdtsc,
    bool use_high_resolution_timer,
    uint64_t overhead,
    int cpu_affinity,
    bool measure_memory
) {
    // Input preparation deliberately happens before either timer starts.
    setup(input_size, input_case, seed);

    CpuAffinityGuard affinity(cpu_affinity);

    MemorySampler memory_sampler;
    if (measure_memory) {
        memory_sampler.start();
    }

    RDTSC_Timer rdtsc_timer;
    HighResolutionTimer high_resolution_timer;

    if (use_high_resolution_timer) {
        high_resolution_timer.start();
    }
    if (use_rdtsc) {
        rdtsc_timer.start();
    }

    function(input_size);

    if (use_rdtsc) {
        rdtsc_timer.stop();
    }
    if (use_high_resolution_timer) {
        high_resolution_timer.stop();
    }

    BenchmarkResult result;
    if (measure_memory) {
        result.memory = memory_sampler.stop();
        result.private_memory_delta = result.memory.peak_private_increase;
        result.working_set_delta = result.memory.peak_working_set_increase;
    }

    if (use_rdtsc) {
        const uint64_t measured = rdtsc_timer.elapsed();
        result.cycles = measured > overhead ? measured - overhead : 0;
    }
    if (use_high_resolution_timer) {
        result.time_ns = high_resolution_timer.elapsed();
    }
    return result;
}
