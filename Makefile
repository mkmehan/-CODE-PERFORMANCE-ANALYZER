CXX = g++

CXXFLAGS = -std=c++17 -Wall -Wextra -Wpedantic -O2

LDLIBS = -lpsapi -lshell32 -lws2_32

TARGET = analyzer.exe
TEST_TARGET = analyzer_tests.exe

SOURCES = main.cpp \
          Benchmark.cpp \
          BenchmarkRunner.cpp \
          ComplexityAnalyzer.cpp \
          HighResolutionTimer.cpp \
          Statistics.cpp \
          SystemInfo.cpp \
          RDTSC_Timer.cpp \
          MemoryMonitor.cpp \
          CpuAffinity.cpp \
          FileInputLoader.cpp \
          analysis/ReportLoader.cpp \
          analysis/HistoryManager.cpp \
          analysis/ComparisonAnalyzer.cpp \
          analysis/RegressionAnalyzer.cpp \
          analysis/TrendAnalyzer.cpp \
          reporting/HtmlReportGenerator.cpp \
          server/DashboardServer.cpp \
          custom/InterfaceDetector.cpp \
          custom/CustomBenchmarkCompiler.cpp \
          custom/CustomBenchmarkRunner.cpp \
          benchmarks/RegisterBenchmarks.cpp \
          benchmarks/BubbleSortBenchmark.cpp \
          benchmarks/InsertionSortBenchmark.cpp \
          benchmarks/SelectionSortBenchmark.cpp \
          benchmarks/MergeSortBenchmark.cpp \
          benchmarks/QuickSortBenchmark.cpp \
          benchmarks/HeapSortBenchmark.cpp \
          benchmarks/StdSortBenchmark.cpp

OBJECTS = $(SOURCES:.cpp=.o)

.PHONY: all clean rebuild test

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(OBJECTS) -o $@ $(LDLIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

rebuild: clean all

test: $(TEST_TARGET)
	.\$(TEST_TARGET)

$(TEST_TARGET): tests/test_main.cpp \
               Benchmark.cpp \
               BenchmarkRunner.cpp \
               ComplexityAnalyzer.cpp \
               HighResolutionTimer.cpp \
               Statistics.cpp \
               SystemInfo.cpp \
               RDTSC_Timer.cpp \
               MemoryMonitor.cpp \
               CpuAffinity.cpp \
               FileInputLoader.cpp \
               analysis/ReportLoader.cpp \
               analysis/HistoryManager.cpp \
               analysis/ComparisonAnalyzer.cpp \
               analysis/RegressionAnalyzer.cpp \
               analysis/TrendAnalyzer.cpp \
               reporting/HtmlReportGenerator.cpp \
               server/DashboardServer.cpp \
               custom/InterfaceDetector.cpp \
               custom/CustomBenchmarkCompiler.cpp \
               custom/CustomBenchmarkRunner.cpp \
               benchmarks/RegisterBenchmarks.cpp \
               benchmarks/BubbleSortBenchmark.cpp \
               benchmarks/InsertionSortBenchmark.cpp \
               benchmarks/SelectionSortBenchmark.cpp \
               benchmarks/MergeSortBenchmark.cpp \
               benchmarks/QuickSortBenchmark.cpp \
               benchmarks/HeapSortBenchmark.cpp \
               benchmarks/StdSortBenchmark.cpp
	$(CXX) $(CXXFLAGS) tests/test_main.cpp \
	       Benchmark.cpp \
	       BenchmarkRunner.cpp \
	       ComplexityAnalyzer.cpp \
	       HighResolutionTimer.cpp \
	       Statistics.cpp \
	       SystemInfo.cpp \
	       RDTSC_Timer.cpp \
	       MemoryMonitor.cpp \
	       CpuAffinity.cpp \
	       FileInputLoader.cpp \
	       analysis/ReportLoader.cpp \
	       analysis/HistoryManager.cpp \
	       analysis/ComparisonAnalyzer.cpp \
	       analysis/RegressionAnalyzer.cpp \
	       analysis/TrendAnalyzer.cpp \
	       reporting/HtmlReportGenerator.cpp \
	       server/DashboardServer.cpp \
	       custom/InterfaceDetector.cpp \
	       custom/CustomBenchmarkCompiler.cpp \
	       custom/CustomBenchmarkRunner.cpp \
	       benchmarks/RegisterBenchmarks.cpp \
	       benchmarks/BubbleSortBenchmark.cpp \
	       benchmarks/InsertionSortBenchmark.cpp \
	       benchmarks/SelectionSortBenchmark.cpp \
	       benchmarks/MergeSortBenchmark.cpp \
	       benchmarks/QuickSortBenchmark.cpp \
	       benchmarks/HeapSortBenchmark.cpp \
	       benchmarks/StdSortBenchmark.cpp \
	       -o $@ $(LDLIBS)

clean:
	powershell -Command "Get-ChildItem -Include *.o -Recurse | Remove-Item -Force -ErrorAction SilentlyContinue; Remove-Item $(TARGET), $(TEST_TARGET) -ErrorAction SilentlyContinue"