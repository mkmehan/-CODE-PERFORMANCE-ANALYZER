#include "HistoryManager.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace analysis {

HistoryManager::HistoryManager(std::string history_dir)
    : history_directory(std::move(history_dir)) {}

std::string HistoryManager::sanitize_filename(const std::string& id) const {
    std::string clean = id;
    for (char& c : clean) {
        if (c == ':' || c == '/' || c == '\\' || c == '*' ||
            c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
            c = '_';
        }
    }
    return clean;
}

std::string HistoryManager::save_run(const BenchmarkRun& run) {
    try {
        std::filesystem::create_directories(history_directory);
    } catch (...) {
        return "";
    }

    std::string base = run.run_id.empty() ? ("RUN-" + run.timestamp) : run.run_id;
    std::string filename = "run_" + sanitize_filename(base) + ".json";
    std::filesystem::path full_path = std::filesystem::path(history_directory) / filename;

    if (ReportLoader::save_to_json_file(run, full_path.string())) {
        return full_path.string();
    }
    return "";
}

std::vector<RunMetadata> HistoryManager::list_runs() const {
    std::vector<RunMetadata> runs;
    if (!std::filesystem::exists(history_directory)) {
        return runs;
    }

    for (const auto& entry : std::filesystem::directory_iterator(history_directory)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            const auto load_res = ReportLoader::load_from_json_file(entry.path().string());
            if (load_res.success) {
                RunMetadata meta;
                meta.run_id = load_res.run.run_id;
                meta.timestamp = load_res.run.timestamp;
                meta.input_type = load_res.run.input.type;
                meta.file_path = load_res.run.input.file_path;
                meta.element_count = load_res.run.input.element_count;
                meta.algorithm_count = load_res.run.results.size();
                meta.filepath_on_disk = entry.path().string();

                for (const auto& rec : load_res.run.results) {
                    meta.algorithms.push_back(rec.algorithm_name);
                }
                runs.push_back(std::move(meta));
            }
        }
    }

    // Sort newest first
    std::sort(runs.begin(), runs.end(), [](const RunMetadata& a, const RunMetadata& b) {
        if (a.timestamp != b.timestamp) {
            return a.timestamp > b.timestamp;
        }
        return a.run_id > b.run_id;
    });

    return runs;
}

LoadResult HistoryManager::load_run(const std::string& run_id_or_path) const {
    // 1. Check if it's already an existing file path
    if (std::filesystem::exists(run_id_or_path)) {
        return ReportLoader::load_from_json_file(run_id_or_path);
    }

    // 2. Check directly in history directory with sanitized filename
    std::filesystem::path p1 = std::filesystem::path(history_directory) /
                               ("run_" + sanitize_filename(run_id_or_path) + ".json");
    if (std::filesystem::exists(p1)) {
        return ReportLoader::load_from_json_file(p1.string());
    }

    std::filesystem::path p2 = std::filesystem::path(history_directory) /
                               (sanitize_filename(run_id_or_path) + ".json");
    if (std::filesystem::exists(p2)) {
        return ReportLoader::load_from_json_file(p2.string());
    }

    // 3. Scan directory files for matching run_id inside metadata
    if (std::filesystem::exists(history_directory)) {
        for (const auto& entry : std::filesystem::directory_iterator(history_directory)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                const auto res = ReportLoader::load_from_json_file(entry.path().string());
                if (res.success && res.run.run_id == run_id_or_path) {
                    return res;
                }
            }
        }
    }

    LoadResult not_found;
    not_found.error_message = "Run ID or file not found in history: " + run_id_or_path;
    return not_found;
}

std::optional<BenchmarkRun> HistoryManager::get_latest_run(const std::string& exclude_run_id) const {
    const auto runs = list_runs();
    for (const auto& meta : runs) {
        if (!exclude_run_id.empty() && meta.run_id == exclude_run_id) {
            continue;
        }
        const auto loaded = load_run(meta.filepath_on_disk);
        if (loaded.success) {
            return loaded.run;
        }
    }
    return std::nullopt;
}

void HistoryManager::print_history_table(const std::vector<RunMetadata>& runs) const {
    std::cout << "\n================================================================================\n";
    std::cout << "                               BENCHMARK HISTORY\n";
    std::cout << "================================================================================\n";

    if (runs.empty()) {
        std::cout << "No benchmark history recorded yet in: " << history_directory << "\n";
        std::cout << "Run a benchmark to record your first run.\n";
        std::cout << "================================================================================\n\n";
        return;
    }

    std::cout
        << std::left
        << std::setw(28) << "Run ID"
        << std::setw(22) << "Date & Time"
        << std::setw(18) << "Input Source"
        << "Algorithms\n"
        << "────────────────────────────────────────────────────────────────────────────────\n";

    for (const auto& run : runs) {
        std::string source_desc;
        if (run.input_type == "custom_file") {
            source_desc = "File (" + std::to_string(run.element_count) + ")";
        } else {
            source_desc = "Generated (" + std::to_string(run.element_count) + ")";
        }

        std::string algos_summary;
        if (run.algorithms.empty()) {
            algos_summary = "0 algos";
        } else if (run.algorithms.size() == 1) {
            algos_summary = run.algorithms.front();
        } else if (run.algorithms.size() <= 3) {
            for (size_t i = 0; i < run.algorithms.size(); ++i) {
                if (i > 0) algos_summary += ", ";
                algos_summary += run.algorithms[i];
            }
        } else {
            algos_summary = std::to_string(run.algorithms.size()) + " algorithms";
        }

        std::cout
            << std::left
            << std::setw(28) << run.run_id
            << std::setw(22) << run.timestamp
            << std::setw(18) << source_desc
            << algos_summary << '\n';
    }

    std::cout << "================================================================================\n";
    std::cout << "Total runs recorded: " << runs.size() << "\n\n";
}

void HistoryManager::print_run_details(const BenchmarkRun& run) const {
    std::cout << "\n================================================================================\n";
    std::cout << "BENCHMARK RUN DETAILS: " << run.run_id << "\n";
    std::cout << "================================================================================\n";
    std::cout << "Timestamp           : " << run.timestamp << "\n";
    std::cout << "Format Version      : " << run.format_version << "\n";
    std::cout << "System CPU          : " << run.system.cpu << "\n";
    std::cout << "Operating System    : " << run.system.os << "\n";
    std::cout << "Compiler            : " << run.system.compiler << " (" << run.system.optimization << ")\n";

    if (run.input.type == "custom_file") {
        std::cout << "Input Type          : Custom File (" << run.input.file_path << ")\n";
        std::cout << "Element Count       : " << run.input.element_count << "\n";
        std::cout << "Distinct Elements   : " << run.input.distinct_count << "\n";
        std::cout << "Duplicate Count     : " << run.input.duplicate_count << "\n";
        std::cout << "Ordering            : " << run.input.order_description << "\n";
    } else {
        std::cout << "Input Type          : Generated\n";
    }

    std::cout << "Iterations          : " << run.configuration.iterations
              << " (Warmup: " << run.configuration.warmup_runs << ")\n";
    std::cout << "────────────────────────────────────────────────────────────────────────────────\n";
    std::cout
        << std::left
        << std::setw(18) << "Algorithm"
        << std::setw(16) << "Input Type"
        << std::setw(10) << "N"
        << std::setw(16) << "Mean Time"
        << std::setw(16) << "Median Time"
        << "Status\n"
        << "────────────────────────────────────────────────────────────────────────────────\n";

    for (const auto& rec : run.results) {
        std::ostringstream mean_str, median_str;
        if (rec.time_mean_ns >= 1e6) {
            mean_str << std::fixed << std::setprecision(2) << (rec.time_mean_ns / 1e6) << " ms";
            median_str << std::fixed << std::setprecision(2) << (rec.time_median_ns / 1e6) << " ms";
        } else if (rec.time_mean_ns >= 1e3) {
            mean_str << std::fixed << std::setprecision(2) << (rec.time_mean_ns / 1e3) << " µs";
            median_str << std::fixed << std::setprecision(2) << (rec.time_median_ns / 1e3) << " µs";
        } else {
            mean_str << std::fixed << std::setprecision(0) << rec.time_mean_ns << " ns";
            median_str << std::fixed << std::setprecision(0) << rec.time_median_ns << " ns";
        }

        std::cout
            << std::left
            << std::setw(18) << rec.algorithm_name
            << std::setw(16) << rec.input_type
            << std::setw(10) << rec.input_size
            << std::setw(16) << mean_str.str()
            << std::setw(16) << median_str.str()
            << (rec.verified ? "PASS ✓" : "FAIL") << '\n';
    }
    std::cout << "================================================================================\n\n";
}

} // namespace analysis

