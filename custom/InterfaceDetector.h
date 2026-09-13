#ifndef CUSTOM_INTERFACE_DETECTOR_H
#define CUSTOM_INTERFACE_DETECTOR_H

#include "CustomAlgorithm.h"

#include <string>
#include <vector>

namespace custom {

enum class SearchInterfaceType {
    Unknown,
    PointerSizeTarget,       // int func(const int* data, int/size_t size, int target)
    PointerTargetSize,       // int func(const int* data, int target, int/size_t size)
    ArraySizeTarget,         // int func(int arr[], int/size_t n, int target)
    VectorRefTarget,         // int func(const std::vector<int>& data, int target)
    VectorValTarget,         // int func(std::vector<int> data, int target)
    VectorTargetFirst,       // int func(int target, const std::vector<int>& data)
    ExternCSearchAlgorithm   // extern "C" int search_algorithm(...)
};

std::string search_interface_type_to_string(SearchInterfaceType type);

struct DetectionResult {
    bool recognized = false;
    CustomCategory category = CustomCategory::Search;
    SearchInterfaceType interface_type = SearchInterfaceType::Unknown;
    std::string detected_function_name;
    std::string detected_signature;
    std::string return_type;
    std::string suggested_display_name;
    std::string generated_adapter_code;
    std::string diagnostic_message;
};

class InterfaceDetector {
public:
    // Detects signature from raw C++ source code text
    static DetectionResult detect(const std::string& source_code, CustomCategory category = CustomCategory::Search);

    // Detects signature directly from a C++ file path on disk
    static DetectionResult detect_from_file(const std::string& file_path, CustomCategory category = CustomCategory::Search);

    // Generates an adapter C++ source file that wraps the user function into ISearchAlgorithm
    static std::string generate_search_adapter(
        const std::string& user_func_name,
        SearchInterfaceType iface_type,
        const std::string& display_name,
        const std::string& class_name = "GeneratedSearchAdapter"
    );

private:
    static std::string strip_comments_and_strings(const std::string& code);
    static DetectionResult detect_search_interface(const std::string& clean_code);
};

} // namespace custom

#endif // CUSTOM_INTERFACE_DETECTOR_H

