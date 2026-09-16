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
        case SearchInterfaceType::DatasetOnly:
            return "int (const int* data, size_t size)";
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

    // 1. Guard against standalone main() executable programs
    if (std::regex_search(clean_code, std::regex(R"(\bint\s+main\s*\()"))) {
        res.recognized = false;
        res.interface_type = SearchInterfaceType::Unknown;
        res.diagnostic_message = "Standalone main() detected. Custom Benchmark expects an algorithm function implementation, not a complete executable program. Please remove main() and provide only the search function.";
        return res;
    }

    // 2. Check for strict extern "C" search_algorithm declaration
    std::regex extern_c_regex(R"(\bextern\s+"C"\s+(?:int|int32_t)\s+search_algorithm\s*\([^)]*\))");
    if (std::regex_search(clean_code, extern_c_regex)) {
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

    struct ScoredCandidate {
        DetectionResult result;
        int score = 0;
    };
    std::vector<ScoredCandidate> candidates;

    for (auto it = words_begin; it != words_end; ++it) {
        std::smatch match = *it;
        std::string ret_type = match[1].str();
        std::string func_name = match[2].str();
        std::string params = match[3].str();

        // Skip main or irrelevant lifecycle methods
        if (func_name == "main" || func_name == "test" || func_name == "setup" || func_name == "verify") {
            continue;
        }

        // Normalize params (lowercase for detection)
        std::string lower_params = params;
        std::transform(lower_params.begin(), lower_params.end(), lower_params.begin(), [](unsigned char c){ return std::tolower(c); });

        std::string lower_func = func_name;
        std::transform(lower_func.begin(), lower_func.end(), lower_func.begin(), [](unsigned char c){ return std::tolower(c); });

        int score = 0;
        if (lower_func.find("search") != std::string::npos) score += 60;
        if (lower_func.find("find") != std::string::npos) score += 40;
        if (lower_func.find("binary") != std::string::npos) score += 30;
        if (lower_func.find("linear") != std::string::npos) score += 30;
        if (lower_func.find("lookup") != std::string::npos) score += 25;
        if (lower_func.find("helper") != std::string::npos || lower_func.find("util") != std::string::npos || lower_func.find("aux") != std::string::npos) score -= 40;

        DetectionResult cand;
        cand.category = CustomCategory::Search;

        // Check Pattern A: vector<int>
        if (lower_params.find("vector") != std::string::npos && lower_params.find("int") != std::string::npos) {
            cand.recognized = true;
            cand.detected_function_name = func_name;
            cand.return_type = ret_type;
            cand.detected_signature = ret_type + " " + func_name + "(" + params + ")";
            cand.suggested_display_name = format_display_name(func_name);

            if (lower_params.find("target") != std::string::npos && lower_params.find("target") < lower_params.find("vector")) {
                cand.interface_type = SearchInterfaceType::VectorTargetFirst;
                cand.diagnostic_message = "Recognized vector search with target parameter first";
            } else if (lower_params.find('&') != std::string::npos) {
                cand.interface_type = SearchInterfaceType::VectorRefTarget;
                cand.diagnostic_message = "Recognized standard pass-by-reference vector search function";
            } else {
                cand.interface_type = SearchInterfaceType::VectorValTarget;
                cand.diagnostic_message = "Recognized pass-by-value vector search function";
            }
            candidates.push_back({cand, score});
            continue;
        }

        // Check Pattern B: int arr[] or int data[]
        if (lower_params.find("[]") != std::string::npos && lower_params.find("int") != std::string::npos) {
            cand.recognized = true;
            cand.detected_function_name = func_name;
            cand.return_type = ret_type;
            cand.detected_signature = ret_type + " " + func_name + "(" + params + ")";
            cand.suggested_display_name = format_display_name(func_name);
            cand.interface_type = SearchInterfaceType::ArraySizeTarget;
            cand.diagnostic_message = "Recognized classic C-array search function (int arr[], int n, int target)";
            candidates.push_back({cand, score});
            continue;
        }

        // Check Pattern C: int* pointer
        if (lower_params.find("int*") != std::string::npos || lower_params.find("int *") != std::string::npos) {
            cand.recognized = true;
            cand.detected_function_name = func_name;
            cand.return_type = ret_type;
            cand.detected_signature = ret_type + " " + func_name + "(" + params + ")";
            cand.suggested_display_name = format_display_name(func_name);

            // Check if target appears before size
            size_t ptr_pos = lower_params.find("int*");
            if (ptr_pos == std::string::npos) ptr_pos = lower_params.find("int *");
            size_t target_pos = lower_params.find("target");
            size_t size_pos = lower_params.find("size");
            if (size_pos == std::string::npos) size_pos = lower_params.find("len");
            if (size_pos == std::string::npos) size_pos = lower_params.find("count");

            if (target_pos != std::string::npos && size_pos != std::string::npos && target_pos < size_pos) {
                cand.interface_type = SearchInterfaceType::PointerTargetSize;
                cand.diagnostic_message = "Recognized pointer search with target before size";
            } else {
                cand.interface_type = SearchInterfaceType::PointerSizeTarget;
                cand.diagnostic_message = "Recognized pointer & size search function (const int*, size, target)";
            }
            candidates.push_back({cand, score});
            continue;
        }
    }

    if (!candidates.empty()) {
        std::sort(candidates.begin(), candidates.end(), [](const ScoredCandidate& a, const ScoredCandidate& b) {
            return a.score > b.score;
        });
        return candidates.front().result;
    }

    res.recognized = false;
    res.interface_type = SearchInterfaceType::Unknown;
    res.diagnostic_message = "Function interface not automatically recognized. Please provide a supported search interface.";
    return res;
}

DetectionResult InterfaceDetector::detect(const std::string& source_code, CustomCategory category, const std::string& interface_mode) {
    std::string clean = strip_comments_and_strings(source_code);
    DetectionResult res;
    res.category = category;

    // 1. Guard against standalone main() executable programs
    if (std::regex_search(clean, std::regex(R"(\bint\s+main\s*\()"))) {
        res.recognized = false;
        res.interface_type = SearchInterfaceType::Unknown;
        res.diagnostic_message = "Standalone main() detected. Custom Benchmark expects an algorithm function implementation, not a complete executable program. Please remove main() and provide only the required function.";
        return res;
    }

    if (category == CustomCategory::Search) {
        if (interface_mode == "dataset_only") {
            // Check for search_algorithm(const int* data, int size) or similar
            std::regex ds_regex(R"(\b(int|int32_t|size_t|void)\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*\(\s*(?:const\s+int\s*\*|const\s+std::vector<int>&|int\s*\*)[^)]*\)\s*\{)");
            std::smatch m;
            if (std::regex_search(clean, m, ds_regex)) {
                res.recognized = true;
                res.interface_type = SearchInterfaceType::DatasetOnly;
                res.return_type = m[1].str();
                res.detected_function_name = m[2].str();
                res.detected_signature = "int " + res.detected_function_name + "(const int* data, int size)";
                res.suggested_display_name = format_display_name(res.detected_function_name);
                res.diagnostic_message = "✓ Interface valid: Dataset-only search function recognized.";
            } else {
                res.recognized = false;
                res.diagnostic_message = "✗ Invalid Interface. Required: int search_algorithm(const int* data, int size). Please modify your function to match the required template.";
            }
        } else {
            res = detect_search_interface(clean);
            if (!res.recognized && res.diagnostic_message.find("main()") == std::string::npos) {
                res.diagnostic_message = "✗ Invalid Interface. Required: int search_algorithm(const int* data, int size, int target). Please modify your function to match the required template.";
            }
        }
    } else if (category == CustomCategory::Matrix) {
        // Contract: void matrix_multiply(const double* A, const double* B, double* C, int N) or void matrix_operation(...)
        std::smatch m;
        if (std::regex_search(clean, m, std::regex(R"(\b(?:void)\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*\([^)]*double[^)]*\))"))) {
            res.recognized = true;
            res.detected_function_name = m[1].str();
            res.return_type = "void";
            res.detected_signature = "void " + res.detected_function_name + "(const double* A, const double* B, double* C, int N)";
            res.suggested_display_name = format_display_name(res.detected_function_name);
            res.diagnostic_message = "✓ Contract valid: Matrix operation function recognized (Extension Preview).";
        } else {
            res.recognized = false;
            res.diagnostic_message = "✗ Required contract: void matrix_multiply(const double* A, const double* B, double* C, int N). Please modify your function to match the required template.";
        }
    } else if (category == CustomCategory::Graph) {
        // Contract: int graph_traverse(const int* adj, const int* offsets, int V, int start_node) or void graph_algorithm(...)
        std::smatch m;
        if (std::regex_search(clean, m, std::regex(R"(\b(?:void|int)\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*\([^)]*(?:adj|offsets|edges|graph|vertices)[^)]*\))"))) {
            res.recognized = true;
            res.detected_function_name = m[1].str();
            res.return_type = m[0].str().find("int") == 0 ? "int" : "void";
            res.detected_signature = res.return_type + " " + res.detected_function_name + "(const int* adj, const int* offsets, int V, int start_node)";
            res.suggested_display_name = format_display_name(res.detected_function_name);
            res.diagnostic_message = "✓ Contract valid: Graph algorithm recognized (Extension Preview).";
        } else {
            res.recognized = false;
            res.diagnostic_message = "✗ Required contract: int graph_traverse(const int* adj, const int* offsets, int V, int start_node). Please modify your function to match the required template.";
        }
    } else if (category == CustomCategory::Sorting) {
        // Contract: void custom_sort(int* data, int size) or void sort_algorithm(...)
        std::smatch m;
        if (std::regex_search(clean, m, std::regex(R"(\b(?:void)\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*\([^)]*int\s*\*\s*[^)]*\))"))) {
            res.recognized = true;
            res.detected_function_name = m[1].str();
            res.return_type = "void";
            res.detected_signature = "void " + res.detected_function_name + "(int* data, int size)";
            res.suggested_display_name = format_display_name(res.detected_function_name);
            res.diagnostic_message = "✓ Contract valid: Custom sort function recognized (Extension Preview).";
        } else {
            res.recognized = false;
            res.diagnostic_message = "✗ Required contract: void custom_sort(int* data, int size). Please modify your function to match the required template.";
        }
    } else {
        res.recognized = false;
        res.diagnostic_message = "Custom category not recognized.";
    }

    if (res.recognized && category == CustomCategory::Search) {
        res.generated_adapter_code = generate_search_adapter(
            res.detected_function_name,
            res.interface_type,
            res.suggested_display_name
        );
    }
    return res;
}

DetectionResult InterfaceDetector::detect_from_file(const std::string& file_path, CustomCategory category, const std::string& interface_mode) {
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
    return detect(ss.str(), category, interface_mode);
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
        case SearchInterfaceType::DatasetOnly:
            ss << "int " << user_func_name << "(const int*, int);\n\n";
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
        case SearchInterfaceType::DatasetOnly:
            ss << "        (void)target;\n";
            ss << "        return " << user_func_name << "(data, static_cast<int>(size));\n";
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

