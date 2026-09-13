#include "InterfaceDetector.h"

#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm>
#include <cctype>

namespace custom {

std::string search_interface_type_to_string(SearchInterfaceType type) {
    switch (type) {
        case SearchInterfaceType::PointerSizeTarget:
            return "int (const int* data, size_t size, int target)";
        case SearchInterfaceType::PointerTargetSize:
            return "int (const int* data, int target, size_t size)";
        case SearchInterfaceType::ArraySizeTarget:
            return "int (int arr[], int n, int target)";
        case SearchInterfaceType::VectorRefTarget:
            return "int (const std::vector<int>& data, int target)";
        case SearchInterfaceType::VectorValTarget:
            return "int (std::vector<int> data, int target)";
        case SearchInterfaceType::VectorTargetFirst:
            return "int (int target, const std::vector<int>& data)";
        case SearchInterfaceType::ExternCSearchAlgorithm:
            return "extern \"C\" int search_algorithm(const int* data, int size, int target)";
        case SearchInterfaceType::Unknown:
            return "Unknown / Custom Interface";
    }
    return "Unknown";
}

namespace {

std::string format_display_name(const std::string& raw_name) {
    if (raw_name.empty()) return "Custom Algorithm";
    std::string result;
    for (size_t i = 0; i < raw_name.size(); ++i) {
        char c = raw_name[i];
        if (c == '_' || c == '-') {
            if (!result.empty() && result.back() != ' ') result += ' ';
        } else if (std::isupper(static_cast<unsigned char>(c))) {
            if (!result.empty() && result.back() != ' ') result += ' ';
            result += c;
        } else if (i == 0 || result.back() == ' ') {
            result += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        } else {
            result += c;
        }
    }
    return result;
}

} // namespace

std::string InterfaceDetector::strip_comments_and_strings(const std::string& code) {
    std::string clean;
    clean.reserve(code.size());
    size_t i = 0;
    const size_t n = code.size();

    while (i < n) {
        // Line comment
        if (code[i] == '/' && i + 1 < n && code[i + 1] == '/') {
            i += 2;
            while (i < n && code[i] != '\n') ++i;
            if (i < n) clean += '\n';
        }
        // Block comment
        else if (code[i] == '/' && i + 1 < n && code[i + 1] == '*') {
            i += 2;
            while (i + 1 < n && !(code[i] == '*' && code[i + 1] == '/')) ++i;
            if (i + 1 < n) i += 2; // skip */
        }
        // String literal
        else if (code[i] == '"') {
            clean += "\"\"";
            ++i;
            while (i < n && code[i] != '"') {
                if (code[i] == '\\' && i + 1 < n) i += 2;
                else ++i;
            }
            if (i < n) ++i;
        }
        // Character literal
        else if (code[i] == '\'') {
            clean += "''";
            ++i;
            while (i < n && code[i] != '\'') {
                if (code[i] == '\\' && i + 1 < n) i += 2;
                else ++i;
            }
            if (i < n) ++i;
        } else {
            clean += code[i++];
        }
    }
    return clean;
}

DetectionResult InterfaceDetector::detect_search_interface(const std::string& clean_code) {
    DetectionResult res;
    res.category = CustomCategory::Search;

    // 1. Check for extern "C" search_algorithm
    if (clean_code.find("extern \"C\"") != std::string::npos && clean_code.find("search_algorithm") != std::string::npos) {
        res.recognized = true;
        res.interface_type = SearchInterfaceType::ExternCSearchAlgorithm;
        res.detected_function_name = "search_algorithm";
        res.return_type = "int";
        res.detected_signature = "extern \"C\" int search_algorithm(...)";
        res.suggested_display_name = "Search Algorithm";
        res.diagnostic_message = "Standard C-linkage search_algorithm function recognized";
        return res;
    }

    // Regex to match function definitions:
    // return_type identifier ( params ) {
    std::regex func_regex(
        R"((int|int32_t|size_t|ssize_t|long)\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*\(([^)]*)\)\s*\{)"
    );

    auto words_begin = std::sregex_iterator(clean_code.begin(), clean_code.end(), func_regex);
    auto words_end = std::sregex_iterator();

    for (auto it = words_begin; it != words_end; ++it) {
        std::smatch match = *it;
        std::string ret_type = match[1].str();
        std::string func_name = match[2].str();
        std::string params = match[3].str();

        // Skip main or irrelevant methods
        if (func_name == "main" || func_name == "test" || func_name == "setup" || func_name == "verify") {
            continue;
        }

        // Normalize params (lowercase for detection)
        std::string lower_params = params;
        std::transform(lower_params.begin(), lower_params.end(), lower_params.begin(), [](unsigned char c){ return std::tolower(c); });

        // Check Pattern A: vector<int>
        if (lower_params.find("vector") != std::string::npos && lower_params.find("int") != std::string::npos) {
            res.recognized = true;
            res.detected_function_name = func_name;
            res.return_type = ret_type;
            res.detected_signature = ret_type + " " + func_name + "(" + params + ")";
            res.suggested_display_name = format_display_name(func_name);

            if (lower_params.find("target") != std::string::npos && lower_params.find("target") < lower_params.find("vector")) {
                res.interface_type = SearchInterfaceType::VectorTargetFirst;
                res.diagnostic_message = "Recognized vector search with target parameter first";
            } else if (lower_params.find('&') != std::string::npos) {
                res.interface_type = SearchInterfaceType::VectorRefTarget;
                res.diagnostic_message = "Recognized standard pass-by-reference vector search function";
            } else {
                res.interface_type = SearchInterfaceType::VectorValTarget;
                res.diagnostic_message = "Recognized pass-by-value vector search function";
            }
            return res;
        }

        // Check Pattern B: int arr[] or int data[]
        if (lower_params.find("[]") != std::string::npos && lower_params.find("int") != std::string::npos) {
            res.recognized = true;
            res.detected_function_name = func_name;
            res.return_type = ret_type;
            res.detected_signature = ret_type + " " + func_name + "(" + params + ")";
            res.suggested_display_name = format_display_name(func_name);
            res.interface_type = SearchInterfaceType::ArraySizeTarget;
            res.diagnostic_message = "Recognized classic C-array search function (int arr[], int n, int target)";
            return res;
        }

        // Check Pattern C: int* pointer
        if (lower_params.find("int*") != std::string::npos || lower_params.find("int *") != std::string::npos) {
            res.recognized = true;
            res.detected_function_name = func_name;
            res.return_type = ret_type;
            res.detected_signature = ret_type + " " + func_name + "(" + params + ")";
            res.suggested_display_name = format_display_name(func_name);

            // Check if target appears before size
            size_t ptr_pos = lower_params.find("int*");
            if (ptr_pos == std::string::npos) ptr_pos = lower_params.find("int *");
            size_t target_pos = lower_params.find("target");
            size_t size_pos = lower_params.find("size");
            if (size_pos == std::string::npos) size_pos = lower_params.find("len");
            if (size_pos == std::string::npos) size_pos = lower_params.find("count");

            if (target_pos != std::string::npos && size_pos != std::string::npos && target_pos < size_pos) {
                res.interface_type = SearchInterfaceType::PointerTargetSize;
                res.diagnostic_message = "Recognized pointer search with target before size";
            } else {
                res.interface_type = SearchInterfaceType::PointerSizeTarget;
                res.diagnostic_message = "Recognized pointer & size search function (const int*, size, target)";
            }
            return res;
        }
    }

    res.recognized = false;
    res.interface_type = SearchInterfaceType::Unknown;
    res.diagnostic_message = "Function interface not automatically recognized. Please provide a custom adapter.";
    return res;
}

DetectionResult InterfaceDetector::detect(const std::string& source_code, CustomCategory category) {
    std::string clean = strip_comments_and_strings(source_code);
    DetectionResult res;
    if (category == CustomCategory::Search) {
        res = detect_search_interface(clean);
    } else {
        res.recognized = false;
        res.category = category;
        res.interface_type = SearchInterfaceType::Unknown;
        res.diagnostic_message = "Domain not yet automated; please provide custom adapter.";
    }

    if (res.recognized) {
        res.generated_adapter_code = generate_search_adapter(
            res.detected_function_name,
            res.interface_type,
            res.suggested_display_name
        );
    }
    return res;
}

DetectionResult InterfaceDetector::detect_from_file(const std::string& file_path, CustomCategory category) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        DetectionResult err;
        err.recognized = false;
        err.category = category;
        err.diagnostic_message = "Failed to open file: " + file_path;
        return err;
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return detect(ss.str(), category);
}

std::string InterfaceDetector::generate_search_adapter(
    const std::string& user_func_name,
    SearchInterfaceType iface_type,
    const std::string& display_name,
    const std::string& class_name
) {
    std::ostringstream ss;
    ss << "#include \"custom/CustomAlgorithm.h\"\n";
    ss << "#include <vector>\n";
    ss << "#include <cstddef>\n\n";

    // Forward declarations based on interface type
    switch (iface_type) {
        case SearchInterfaceType::VectorRefTarget:
            ss << "int " << user_func_name << "(const std::vector<int>&, int);\n\n";
            break;
        case SearchInterfaceType::VectorValTarget:
            ss << "int " << user_func_name << "(std::vector<int>, int);\n\n";
            break;
        case SearchInterfaceType::VectorTargetFirst:
            ss << "int " << user_func_name << "(int, const std::vector<int>&);\n\n";
            break;
        case SearchInterfaceType::PointerSizeTarget:
            ss << "int " << user_func_name << "(const int*, size_t, int);\n\n";
            break;
        case SearchInterfaceType::PointerTargetSize:
            ss << "int " << user_func_name << "(const int*, int, size_t);\n\n";
            break;
        case SearchInterfaceType::ArraySizeTarget:
            ss << "int " << user_func_name << "(int arr[], int, int);\n\n";
            break;
        case SearchInterfaceType::ExternCSearchAlgorithm:
            ss << "extern \"C\" int search_algorithm(const int*, int, int);\n\n";
            break;
        case SearchInterfaceType::Unknown:
            ss << "// Custom or unknown interface\n\n";
            break;
    }

    ss << "namespace custom {\n\n";
    ss << "class " << class_name << " : public ISearchAlgorithm {\n";
    ss << "private:\n";

    // Add cached vector for vector-based algorithms so vector allocation happens ONCE in prepare(), not inside timed search!
    if (iface_type == SearchInterfaceType::VectorRefTarget ||
        iface_type == SearchInterfaceType::VectorValTarget ||
        iface_type == SearchInterfaceType::VectorTargetFirst) {
        ss << "    std::vector<int> cached_vec;\n";
    }

    ss << "public:\n";
    ss << "    std::string name() const override { return \"" << display_name << "\"; }\n\n";

    if (iface_type == SearchInterfaceType::VectorRefTarget ||
        iface_type == SearchInterfaceType::VectorValTarget ||
        iface_type == SearchInterfaceType::VectorTargetFirst) {
        ss << "    void prepare(const int* data, size_t size) override {\n";
        ss << "        cached_vec.assign(data, data + size);\n";
        ss << "    }\n\n";
    }

    ss << "    int search(const int* data, size_t size, int target) override {\n";

    switch (iface_type) {
        case SearchInterfaceType::VectorRefTarget:
        case SearchInterfaceType::VectorValTarget:
            ss << "        return " << user_func_name << "(cached_vec, target);\n";
            break;
        case SearchInterfaceType::VectorTargetFirst:
            ss << "        return " << user_func_name << "(target, cached_vec);\n";
            break;
        case SearchInterfaceType::PointerSizeTarget:
            ss << "        return " << user_func_name << "(data, size, target);\n";
            break;
        case SearchInterfaceType::PointerTargetSize:
            ss << "        return " << user_func_name << "(data, target, size);\n";
            break;
        case SearchInterfaceType::ArraySizeTarget:
            ss << "        return " << user_func_name << "(const_cast<int*>(data), static_cast<int>(size), target);\n";
            break;
        case SearchInterfaceType::ExternCSearchAlgorithm:
            ss << "        return search_algorithm(data, static_cast<int>(size), target);\n";
            break;
        case SearchInterfaceType::Unknown:
            ss << "        return -1; // Unknown interface\n";
            break;
    }

    ss << "    }\n";
    ss << "};\n\n";
    ss << "} // namespace custom\n";

    return ss.str();
}

} // namespace custom

