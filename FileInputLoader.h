#ifndef FILE_INPUT_LOADER_H
#define FILE_INPUT_LOADER_H

#include <cstddef>
#include <string>
#include <vector>

struct DatasetInfo {
    size_t element_count = 0;
    size_t distinct_count = 0;
    size_t duplicate_count = 0;
    int min_value = 0;
    int max_value = 0;

    bool is_sorted = false;
    bool is_reverse_sorted = false;
    bool is_all_equal = false;
    std::string order_description; // "Sorted", "Reverse Sorted", "All Equal", "Unsorted"
};

struct ValidationResult {
    bool valid = false;
    std::string error_message;
    size_t error_line = 0;
    std::string expected;
    std::string found;

    std::vector<int> data;
    DatasetInfo info;
};

class FileInputLoader {
public:
    // Reads, parses, and validates the file at filepath.
    // Does not throw exceptions; reports all diagnostic details in ValidationResult.
    static ValidationResult load_and_validate(const std::string& filepath);

    // In-memory cache for active custom dataset during benchmarking
    static void set_active_dataset(
        std::vector<int> data,
        DatasetInfo info,
        std::string filepath
    );
    static const std::vector<int>& get_active_dataset();
    static const DatasetInfo& get_active_dataset_info();
    static const std::string& get_active_file_path();
    static bool has_active_dataset();
    static void clear_active_dataset();

    // Pretty-printing utilities for terminal
    static void print_validation_card(const ValidationResult& result);
    static void print_dataset_properties(
        const DatasetInfo& info,
        const std::string& filepath,
        const std::vector<int>& data
    );
};

#endif
