# V3 Resource Features

## Automated tests
Run `mingw32-make test` to build and execute `analyzer_tests.exe`.
The suite checks sorting correctness, input generation, statistics, complexity analysis, memory visibility, and CPU affinity.

## Memory measurement
V3 records **Peak observed process memory using repeated sampling** (`PrivateUsage` and `WorkingSetSize`) during algorithm execution, rather than an allocator-level exact peak memory.

A dedicated background sampler thread captures repeated snapshots during algorithm execution, tracking the maximum observed values against the initial baseline snapshot. Timers start *after* the sampler starts and stop *before* the sampler joins, maintaining non-intrusive timing purity.

The results are reported in the `MEMORY ANALYSIS` section:
- **Baseline**: Memory observed at the start of the measured run.
- **Peak observed**: Highest process memory snapshot observed during execution.
- **Peak increase**: Increase from baseline to peak observed.
- **Net change**: Difference between final snapshot and baseline.

Use `--no-memory` to disable memory sampling when timing purity is more important than resource telemetry.

## CPU affinity
Use `--cpu N` to pin the benchmark thread to logical processor `N` (0-based). Use `--cpu 0` for CPU 0. Use `--no-memory` independently of affinity. This V3 implementation uses the Windows thread affinity-mask API for processors addressable by the current group; practical single-group support is 0-63.

When CPU affinity is enabled:
- **Benchmark thread**: pinned to logical CPU `N`.
- **Memory sampler thread**: explicitly configured with full process affinity (no single-core restriction) so it does not compete for execution on the pinned benchmark core.

Example:

    analyzer.exe --all --cpu 3 --sizes 100,500,1000 --iterations 100 --warmup 20

The affinity is applied to the benchmark thread and restored automatically using RAII after each measured run.
