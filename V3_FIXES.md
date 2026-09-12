# V3 Fixes Applied

## Fixed issues

1. `BenchmarkRunner` now uses `BenchmarkConfig` consistently. The old `BenchmarkRunner(10, 100)` call was removed.
2. CLI options are parsed into one configuration before constructing the runner.
3. `--sizes`, `--cases`, `--iterations`, `--warmup`, `--seed`, `--no-rdtsc`, and `--no-qpc` are supported consistently.
4. The Makefile now includes all V3 source files, including `ComplexityAnalyzer`, `HighResolutionTimer`, `SystemInfo`, Heap Sort, and the standard-library sorting benchmarks.
5. RDTSC timer state is initialized safely and remains derived from the common `Timer` interface.
6. CPU information now includes CPU name, architecture, physical cores, and logical processors.
7. Windows version reporting uses `RtlGetVersion` when available.
8. CPU name collection uses `memcpy` rather than type-punning through an `int*`.
9. Complexity analysis explicitly includes its `<algorithm>` dependency.
10. Empty benchmark registration is rejected with a clear error.
11. Generated executables and benchmark reports are ignored by Git.

## Build on Windows

Using MinGW/MSYS2:

```text
mingw32-make clean
mingw32-make
```

Or:

```text
g++ -std=c++17 -Wall -Wextra -Wpedantic -O2 ^
  main.cpp Benchmark.cpp BenchmarkRunner.cpp ComplexityAnalyzer.cpp ^
  HighResolutionTimer.cpp Statistics.cpp SystemInfo.cpp RDTSC_Timer.cpp ^
  benchmarks/RegisterBenchmarks.cpp ^
  benchmarks/BubbleSortBenchmark.cpp ^
  benchmarks/InsertionSortBenchmark.cpp ^
  benchmarks/SelectionSortBenchmark.cpp ^
  benchmarks/MergeSortBenchmark.cpp ^
  benchmarks/QuickSortBenchmark.cpp ^
  benchmarks/HeapSortBenchmark.cpp ^
  benchmarks/StdSortBenchmark.cpp ^
  -o analyzer.exe
```

## Example commands

```text
analyzer.exe --help
analyzer.exe --all --sizes 100,500,1000,2000 --iterations 30 --warmup 5
analyzer.exe --quick --cases random,sorted,nearly-sorted
analyzer.exe --std-sort --sizes 1000,5000,10000 --no-rdtsc
```

Reports are written under `results/` as CSV and JSON files.

## #12 CLI mode collision: `--quick`

Fixed a collision between the top-level `--quick` smoke-test mode and the Quick Sort benchmark selector. `--quick` now runs the full benchmark suite with a fast default workload (`{100, 1000}` sizes, 10 iterations, 2 warmups), while explicit `--sizes`, `--iterations`, and `--warmup` values override those defaults. Quick Sort itself is selected with `--quicksort`. The two modes are mutually exclusive with `--all` and other benchmark selectors.
