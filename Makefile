CXX = g++

CXXFLAGS = -std=c++17 -Wall -Wextra -Wpedantic -O2

LDLIBS = -lpsapi -lshell32

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
               Statistics.cpp \
               ComplexityAnalyzer.cpp \
               HighResolutionTimer.cpp \
               RDTSC_Timer.cpp \
               MemoryMonitor.cpp \
               CpuAffinity.cpp \
               FileInputLoader.cpp \
               analysis/ReportLoader.cpp \
               analysis/HistoryManager.cpp \
               analysis/ComparisonAnalyzer.cpp \
               analysis/RegressionAnalyzer.cpp \
               analysis/TrendAnalyzer.cpp \
               reporting/HtmlReportGenerator.cpp
	$(CXX) $(CXXFLAGS) tests/test_main.cpp \
	       Benchmark.cpp \
	       Statistics.cpp \
	       ComplexityAnalyzer.cpp \
	       HighResolutionTimer.cpp \
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
	       -o $@ $(LDLIBS)

clean:
	cmd /C "del /Q $(TARGET) $(TEST_TARGET) 2>nul"
	cmd /C "del /Q *.o benchmarks\*.o analysis\*.o reporting\*.o tests\*.o 2>nul"