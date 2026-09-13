#ifndef CUSTOM_BENCHMARK_COMPILER_H
#define CUSTOM_BENCHMARK_COMPILER_H

#include "InterfaceDetector.h"

#include <string>
#include <vector>

namespace custom {

struct AlgorithmSourceSpec {
    std::string algorithm_name;
    std::string source_file_path;
    std::string detected_function;
    SearchInterfaceType interface_type;
};

struct CompilationResult {
    bool success = false;
    std::string runner_executable_path;
    std::string compiler_errors;
    std::string generated_adapter_path;
};

class CustomBenchmarkCompiler {
public:
    // Compiles the given algorithm source specifications into an isolated custom_runner.exe binary
    static CompilationResult compile_search_runner(
        const std::vector<AlgorithmSourceSpec>& algorithms,
        const std::string& output_dir = "custom/bin"
    );

private:
    static std::string generate_adapters_translation_unit(
        const std::vector<AlgorithmSourceSpec>& algorithms
    );
};

} // namespace custom

#endif // CUSTOM_BENCHMARK_COMPILER_H

