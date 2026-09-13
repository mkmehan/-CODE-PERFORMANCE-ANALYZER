#ifndef ANALYSIS_HISTORY_MANAGER_H
#define ANALYSIS_HISTORY_MANAGER_H

#include "BenchmarkRun.h"
#include "ReportLoader.h"

#include <optional>
#include <string>
#include <vector>

namespace analysis {

struct RunMetadata {
    std::string run_id;
    std::string timestamp;
    std::string input_type;
    std::string file_path;
    size_t element_count = 0;
    size_t algorithm_count = 0;
    std::vector<std::string> algorithms;
    std::string filepath_on_disk;
};

class HistoryManager {
private:
    std::string history_directory;

    std::string sanitize_filename(const std::string& id) const;

public:
    explicit HistoryManager(std::string history_dir = "results/history");

    const std::string& directory() const { return history_directory; }

    // Save a benchmark run to results/history/run_<run_id>.json
    std::string save_run(const BenchmarkRun& run);

    // List all historical runs sorted with newest first
    std::vector<RunMetadata> list_runs() const;

    // Load a specific historical run by run ID or file path
    LoadResult load_run(const std::string& run_id_or_path) const;

    // Get the most recent run (optionally excluding a specific run_id)
    std::optional<BenchmarkRun> get_latest_run(const std::string& exclude_run_id = "") const;

    // Formatted CLI presentations
    void print_history_table(const std::vector<RunMetadata>& runs) const;
    void print_run_details(const BenchmarkRun& run) const;
};

} // namespace analysis

#endif // ANALYSIS_HISTORY_MANAGER_H

