#include "FileInputLoader.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <unordered_set>

namespace {

const char* const divider =
    "──────────────────────────────────────────────────────────────────────";

std::vector<int> cached_data;
DatasetInfo cached_info;
std::string cached_filepath;
bool dataset_active = false;

std::string comma_fmt(size_t value) {
    std::string result = std::to_string(value);
    for (int position = static_cast<int>(result.size()) - 3; position > 0; position -= 3) {
        result.insert(static_cast<size_t>(position), ",");
    }
    return result;
}

std::string comma_signed_fmt(int value) {
    if (value < 0) {
        return "-" + comma_fmt(static_cast<size_t>(-static_cast<long long>(value)));
    }
    return comma_fmt(static_cast<size_t>(value));
}

bool is_valid_integer(const std::string& str, int& out_value) {
    if (str.empty()) {
        return false;
    }
    size_t idx = 0;
    if (str[0] == '+' || str[0] == '-') {
        if (str.size() == 1) {
            return false;
        }
        idx = 1;
    }
    for (; idx < str.size(); ++idx) {
        if (!std::isdigit(static_cast<unsigned char>(str[idx]))) {
            return false;
        }
    }
    try {
        size_t pos = 0;
        const long long val = std::stoll(str, &pos);
        if (pos != str.size()) {
            return false;
        }
        if (val < std::numeric_limits<int>::min() ||
            val > std::numeric_limits<int>::max()) {
            return false;
        }
        out_value = static_cast<int>(val);
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace

ValidationResult FileInputLoader::load_and_validate(const std::string& filepath) {
    ValidationResult result;

    if (!std::filesystem::exists(filepath)) {
        result.valid = false;
        result.error_line = 0;
        result.error_message = "File does not exist: " + filepath;
        result.expected = "readable file path";
        result.found = "non-existent path";
        return result;
    }

    if (std::filesystem::is_directory(filepath)) {
        result.valid = false;
        result.error_line = 0;
        result.error_message = "Specified path is a directory, not a file: " + filepath;
        result.expected = "text file";
        result.found = "directory";
        return result;
    }

    std::ifstream file(filepath);
    if (!file.is_open()) {
        result.valid = false;
        result.error_line = 0;
        result.error_message = "Unable to open file for reading: " + filepath;
        result.expected = "readable file";
        result.found = "access denied or unreadable";
        return result;
    }

    std::vector<int> values;
    std::string line;
    size_t current_line = 0;
    bool has_any_token = false;

    while (std::getline(file, line)) {
        ++current_line;
        std::istringstream line_stream(line);
        std::string token;

        while (line_stream >> token) {
            has_any_token = true;
            int parsed_value = 0;
            if (!is_valid_integer(token, parsed_value)) {
                result.valid = false;
                result.error_line = current_line;
                result.error_message = "Invalid integer at line " + std::to_string(current_line);
                result.expected = "integer";
                result.found = "\"" + token + "\"";
                return result;
            }
            values.push_back(parsed_value);
        }
    }

    if (!has_any_token || values.empty()) {
        result.valid = false;
        result.error_line = 0;
        result.error_message = "File is empty or contains no elements: " + filepath;
        result.expected = "whitespace-separated integer dataset";
        result.found = "<empty>";
        return result;
    }

    DatasetInfo info;
    info.element_count = values.size();
    info.min_value = *std::min_element(values.begin(), values.end());
    info.max_value = *std::max_element(values.begin(), values.end());

    std::unordered_set<int> unique_elements(values.begin(), values.end());
    info.distinct_count = unique_elements.size();
    info.duplicate_count = info.element_count - info.distinct_count;

    info.is_all_equal = (info.min_value == info.max_value);
    info.is_sorted = std::is_sorted(values.begin(), values.end());
    info.is_reverse_sorted = std::is_sorted(values.rbegin(), values.rend());

    if (info.is_all_equal) {
        info.order_description = "All Equal";
    } else if (info.is_sorted) {
        info.order_description = "Sorted";
    } else if (info.is_reverse_sorted) {
        info.order_description = "Reverse Sorted";
    } else {
        info.order_description = "Unsorted";
    }

    result.valid = true;
    result.info = info;
    result.data = std::move(values);
    return result;
}

void FileInputLoader::set_active_dataset(
    std::vector<int> data,
    DatasetInfo info,
    std::string filepath
) {
    cached_data = std::move(data);
    cached_info = std::move(info);
    cached_filepath = std::move(filepath);
    dataset_active = true;
}

const std::vector<int>& FileInputLoader::get_active_dataset() {
    return cached_data;
}

const DatasetInfo& FileInputLoader::get_active_dataset_info() {
    return cached_info;
}

const std::string& FileInputLoader::get_active_file_path() {
    return cached_filepath;
}

bool FileInputLoader::has_active_dataset() {
    return dataset_active;
}

void FileInputLoader::clear_active_dataset() {
    cached_data.clear();
    cached_info = DatasetInfo{};
    cached_filepath.clear();
    dataset_active = false;
}

void FileInputLoader::print_validation_card(const ValidationResult& result) {
    std::cout
        << "\nFILE VALIDATION\n" << divider << '\n';
    if (result.valid) {
        std::cout
            << "✓ File exists\n"
            << "✓ File readable\n"
            << "✓ Integer values valid\n"
            << "✓ " << comma_fmt(result.info.element_count) << " elements loaded\n"
            << "✓ No parsing errors\n"
            << "✓ Dataset ready\n\n";
    } else {
        std::cout
            << "✗ " << result.error_message << '\n'
            << "✗ Benchmark cancelled\n\n";
        if (!result.expected.empty()) {
            std::cout
                << "Expected: " << result.expected << '\n'
                << "Found   : " << result.found << "\n\n";
        }
    }
}

void FileInputLoader::print_dataset_properties(
    const DatasetInfo& info,
    const std::string& filepath,
    const std::vector<int>& data
) {
    std::cout
        << "CUSTOM DATASET PROPERTIES\n" << divider << '\n'
        << std::left
        << std::setw(20) << "File" << ": " << filepath << '\n'
        << std::setw(20) << "Elements" << ": " << comma_fmt(info.element_count) << '\n'
        << std::setw(20) << "Distinct values" << ": " << comma_fmt(info.distinct_count) << '\n'
        << std::setw(20) << "Duplicates" << ": " << comma_fmt(info.duplicate_count) << '\n'
        << std::setw(20) << "Minimum" << ": " << comma_signed_fmt(info.min_value) << '\n'
        << std::setw(20) << "Maximum" << ": " << comma_signed_fmt(info.max_value) << '\n'
        << std::setw(20) << "Input order" << ": " << info.order_description << "\n\n"
        << "Preview\n" << divider << '\n';

    const size_t preview_count = std::min<size_t>(data.size(), 20);
    for (size_t i = 0; i < preview_count; ++i) {
        if (i > 0) {
            std::cout << "  ";
        }
        std::cout << data[i];
    }
    if (data.size() > preview_count) {
        std::cout << "  ...";
    }
    std::cout << "\n\n";
}
