CXX=g++
CXXFLAGS=-std=c++17 -Wall -O2

TARGET=analyzer.exe

SOURCES=main.cpp \
        Benchmark.cpp \
        BenchmarkRunner.cpp \
        Statistics.cpp \
        RDTSC_Timer.cpp \
        benchmarks/RegisterBenchmarks.cpp \
        benchmarks/BubbleSortBenchmark.cpp \
        benchmarks/InsertionSortBenchmark.cpp \
        benchmarks/SelectionSortBenchmark.cpp \
        benchmarks/MergeSortBenchmark.cpp \
        benchmarks/QuickSortBenchmark.cpp \
        benchmarks/SumBenchmark.cpp

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(TARGET)

clean:
	del /Q $(TARGET) 2>nul