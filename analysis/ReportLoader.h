#ifndef ANALYSIS_REPORT_LOADER_H
#define ANALYSIS_REPORT_LOADER_H

#include "BenchmarkRun.h"

#include <string>

namespace analysis {

struct LoadResult {
    bool success = false;
    std::string error_message;
    BenchmarkRun run;
};

class ReportLoader {
public:
    // Load and validate from JSON file on disk
    static LoadResult load_from_json_file(const std::string& filepath);

    // Load and validate from in-memory JSON text
    static LoadResult load_from_json_string(const std::string& json_text);

    // Serialize BenchmarkRun to formatted JSON string
    static std::string to_json_string(const BenchmarkRun& run);

    // Save BenchmarkRun to formatted JSON file
    static bool save_to_json_file(const BenchmarkRun& run, const std::string& filepath);
};

} // namespace analysis

#endif // ANALYSIS_REPORT_LOADER_H

