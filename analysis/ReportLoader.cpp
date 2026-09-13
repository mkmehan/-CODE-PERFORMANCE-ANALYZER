#include "ReportLoader.h"

#include <cctype>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace analysis {
namespace {

enum class JsonType { Null, Bool, Number, String, Array, Object };

struct JsonValue {
    JsonType type = JsonType::Null;
    bool bool_val = false;
    double num_val = 0.0;
    std::string str_val;
    std::vector<JsonValue> arr_val;
    std::map<std::string, JsonValue> obj_val;

    bool is_null() const { return type == JsonType::Null; }
    bool is_bool() const { return type == JsonType::Bool; }
    bool is_number() const { return type == JsonType::Number; }
    bool is_string() const { return type == JsonType::String; }
    bool is_array() const { return type == JsonType::Array; }
    bool is_object() const { return type == JsonType::Object; }

    std::string as_string(const std::string& def = "") const {
        return is_string() ? str_val : def;
    }

    double as_double(double def = 0.0) const {
        return is_number() ? num_val : def;
    }

    int64_t as_int64(int64_t def = 0) const {
        return is_number() ? static_cast<int64_t>(std::llround(num_val)) : def;
    }

    uint64_t as_uint64(uint64_t def = 0) const {
        if (!is_number() || num_val < 0.0) return def;
        return static_cast<uint64_t>(std::llround(num_val));
    }

    bool as_bool(bool def = false) const {
        return is_bool() ? bool_val : def;
    }

    bool has(const std::string& key) const {
        return is_object() && obj_val.find(key) != obj_val.end();
    }

    const JsonValue* get(const std::string& key) const {
        if (!is_object()) return nullptr;
        auto iter = obj_val.find(key);
        return iter != obj_val.end() ? &iter->second : nullptr;
    }
};

class JsonParser {
private:
    const std::string& src;
    size_t pos = 0;
    size_t line = 1;
    size_t col = 1;

    void advance() {
        if (pos < src.size()) {
            if (src[pos] == '\n') {
                ++line;
                col = 1;
            } else {
                ++col;
            }
            ++pos;
        }
    }

    char peek() const {
        return pos < src.size() ? src[pos] : '\0';
    }

    void skip_whitespace() {
        while (pos < src.size()) {
            char c = src[pos];
            if (std::isspace(static_cast<unsigned char>(c))) {
                advance();
            } else {
                break;
            }
        }
    }

    std::string error(const std::string& message) const {
        return "JSON parse error at line " + std::to_string(line) +
               ", col " + std::to_string(col) + ": " + message;
    }

    bool parse_string(std::string& out, std::string& err) {
        if (peek() != '"') {
            err = error("Expected '\"'");
            return false;
        }
        advance();
        out.clear();
        while (pos < src.size()) {
            char c = peek();
            if (c == '"') {
                advance();
                return true;
            }
            if (c == '\\') {
                advance();
                if (pos >= src.size()) {
                    err = error("Unterminated escape sequence");
                    return false;
                }
                char esc = peek();
                advance();
                switch (esc) {
                    case '"': out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    case '/': out.push_back('/'); break;
                    case 'b': out.push_back('\b'); break;
                    case 'f': out.push_back('\f'); break;
                    case 'n': out.push_back('\n'); break;
                    case 'r': out.push_back('\r'); break;
                    case 't': out.push_back('\t'); break;
                    case 'u': {
                        // 4 hex digits
                        std::string hex;
                        for (int i = 0; i < 4 && pos < src.size(); ++i) {
                            hex.push_back(peek());
                            advance();
                        }
                        try {
                            unsigned long codepoint = std::stoul(hex, nullptr, 16);
                            if (codepoint < 0x80) {
                                out.push_back(static_cast<char>(codepoint));
                            } else {
                                out.push_back('?');
                            }
                        } catch (...) {
                            out.push_back('?');
                        }
                        break;
                    }
                    default:
                        out.push_back(esc);
                        break;
                }
            } else {
                out.push_back(c);
                advance();
            }
        }
        err = error("Unterminated string");
        return false;
    }

    bool parse_number(double& out, std::string& err) {
        size_t start = pos;
        if (peek() == '-') {
            advance();
        }
        if (!std::isdigit(static_cast<unsigned char>(peek()))) {
            err = error("Invalid number");
            return false;
        }
        while (std::isdigit(static_cast<unsigned char>(peek()))) {
            advance();
        }
        if (peek() == '.') {
            advance();
            if (!std::isdigit(static_cast<unsigned char>(peek()))) {
                err = error("Invalid number decimal fraction");
                return false;
            }
            while (std::isdigit(static_cast<unsigned char>(peek()))) {
                advance();
            }
        }
        if (peek() == 'e' || peek() == 'E') {
            advance();
            if (peek() == '+' || peek() == '-') {
                advance();
            }
            if (!std::isdigit(static_cast<unsigned char>(peek()))) {
                err = error("Invalid number exponent");
                return false;
            }
            while (std::isdigit(static_cast<unsigned char>(peek()))) {
                advance();
            }
        }
        try {
            out = std::stod(src.substr(start, pos - start));
            return true;
        } catch (...) {
            err = error("Number out of range");
            return false;
        }
    }

    bool parse_array(JsonValue& val, std::string& err) {
        if (peek() != '[') {
            err = error("Expected '['");
            return false;
        }
        advance();
        val.type = JsonType::Array;
        val.arr_val.clear();
        skip_whitespace();
        if (peek() == ']') {
            advance();
            return true;
        }
        while (pos < src.size()) {
            JsonValue elem;
            if (!parse_value(elem, err)) {
                return false;
            }
            val.arr_val.push_back(std::move(elem));
            skip_whitespace();
            if (peek() == ']') {
                advance();
                return true;
            }
            if (peek() == ',') {
                advance();
                skip_whitespace();
            } else {
                err = error("Expected ',' or ']' in array");
                return false;
            }
        }
        err = error("Unterminated array");
        return false;
    }

    bool parse_object(JsonValue& val, std::string& err) {
        if (peek() != '{') {
            err = error("Expected '{'");
            return false;
        }
        advance();
        val.type = JsonType::Object;
        val.obj_val.clear();
        skip_whitespace();
        if (peek() == '}') {
            advance();
            return true;
        }
        while (pos < src.size()) {
            skip_whitespace();
            if (peek() != '"') {
                err = error("Expected string key in object");
                return false;
            }
            std::string key;
            if (!parse_string(key, err)) {
                return false;
            }
            skip_whitespace();
            if (peek() != ':') {
                err = error("Expected ':' after key in object");
                return false;
            }
            advance();
            skip_whitespace();
            JsonValue field_val;
            if (!parse_value(field_val, err)) {
                return false;
            }
            val.obj_val[key] = std::move(field_val);
            skip_whitespace();
            if (peek() == '}') {
                advance();
                return true;
            }
            if (peek() == ',') {
                advance();
                skip_whitespace();
            } else {
                err = error("Expected ',' or '}' in object");
                return false;
            }
        }
        err = error("Unterminated object");
        return false;
    }

public:
    explicit JsonParser(const std::string& text) : src(text) {}

    bool parse_value(JsonValue& val, std::string& err) {
        skip_whitespace();
        if (pos >= src.size()) {
            err = error("Unexpected end of input");
            return false;
        }
        char c = peek();
        if (c == '"') {
            val.type = JsonType::String;
            return parse_string(val.str_val, err);
        }
        if (c == '{') {
            return parse_object(val, err);
        }
        if (c == '[') {
            return parse_array(val, err);
        }
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) {
            val.type = JsonType::Number;
            return parse_number(val.num_val, err);
        }
        if (src.compare(pos, 4, "true") == 0) {
            val.type = JsonType::Bool;
            val.bool_val = true;
            advance(); advance(); advance(); advance();
            return true;
        }
        if (src.compare(pos, 5, "false") == 0) {
            val.type = JsonType::Bool;
            val.bool_val = false;
            advance(); advance(); advance(); advance(); advance();
            return true;
        }
        if (src.compare(pos, 4, "null") == 0) {
            val.type = JsonType::Null;
            advance(); advance(); advance(); advance();
            return true;
        }
        err = error(std::string("Unexpected token '") + c + "'");
        return false;
    }
};

std::string escape_string(const std::string& str) {
    std::ostringstream out;
    for (char c : str) {
        switch (c) {
            case '"': out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\b': out << "\\b"; break;
            case '\f': out << "\\f"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                        << static_cast<int>(static_cast<unsigned char>(c));
                } else {
                    out << c;
                }
                break;
        }
    }
    return out.str();
}

} // namespace

LoadResult ReportLoader::load_from_json_string(const std::string& json_text) {
    LoadResult result;
    if (json_text.empty()) {
        result.error_message = "Empty JSON input";
        return result;
    }

    JsonParser parser(json_text);
    JsonValue root;
    std::string err;
    if (!parser.parse_value(root, err)) {
        result.error_message = err;
        return result;
    }

    if (!root.is_object()) {
        result.error_message = "Root must be a JSON object";
        return result;
    }

    // 1. Validate format_version
    const JsonValue* fv = root.get("format_version");
    if (!fv || !fv->is_string() || fv->str_val != "4.0") {
        result.error_message = "Unsupported or missing format_version: expected '4.0'";
        return result;
    }
    result.run.format_version = fv->str_val;

    // 2. Validate run_id (optional or required)
    const JsonValue* rid = root.get("run_id");
    if (rid && rid->is_string()) {
        result.run.run_id = rid->str_val;
    }

    // 3. Validate timestamp
    const JsonValue* ts = root.get("timestamp");
    if (!ts) {
        ts = root.get("generated_at"); // backwards compatibility
    }
    if (ts && ts->is_string()) {
        result.run.timestamp = ts->str_val;
    }
    if (result.run.run_id.empty() && !result.run.timestamp.empty()) {
        result.run.run_id = "RUN-" + result.run.timestamp;
    }

    // Benchmark mode and category
    result.run.benchmark_mode = root.has("benchmark_mode") ? root.get("benchmark_mode")->as_string("standard") : "standard";
    result.run.benchmark_category = root.has("benchmark_category") ? root.get("benchmark_category")->as_string("sorting") : "sorting";
    result.run.custom_target_parameter = root.has("custom_target_parameter") ? root.get("custom_target_parameter")->as_string("") : "";

    // 4. Validate system metadata
    const JsonValue* sys = root.get("system");
    if (!sys || !sys->is_object()) {
        result.error_message = "Missing or invalid 'system' object";
        return result;
    }
    if (sys->has("operating_system")) result.run.system.os = sys->get("operating_system")->as_string();
    else if (sys->has("os")) result.run.system.os = sys->get("os")->as_string();
    result.run.system.cpu = sys->has("cpu") ? sys->get("cpu")->as_string() : "";
    result.run.system.architecture = sys->has("architecture") ? sys->get("architecture")->as_string() : "";
    result.run.system.physical_cores = sys->has("physical_cores") ? static_cast<unsigned int>(sys->get("physical_cores")->as_uint64()) : 0;
    result.run.system.logical_cpus = sys->has("logical_processors") ? static_cast<unsigned int>(sys->get("logical_processors")->as_uint64())
                                   : (sys->has("logical_cpus") ? static_cast<unsigned int>(sys->get("logical_cpus")->as_uint64()) : 0);
    result.run.system.compiler = sys->has("compiler") ? sys->get("compiler")->as_string() : "";
    result.run.system.cxx_standard = sys->has("cxx_standard") ? sys->get("cxx_standard")->as_string() : "";
    result.run.system.optimization = sys->has("optimization") ? sys->get("optimization")->as_string() : "";

    // 5. Validate configuration metadata
    const JsonValue* cfg = root.get("configuration");
    if (!cfg || !cfg->is_object()) {
        result.error_message = "Missing or invalid 'configuration' object";
        return result;
    }
    result.run.configuration.warmup_runs = cfg->has("warmup_runs") ? static_cast<int>(cfg->get("warmup_runs")->as_int64()) : 0;
    result.run.configuration.iterations = cfg->has("iterations") ? static_cast<int>(cfg->get("iterations")->as_int64()) : 0;
    result.run.configuration.random_seed = cfg->has("random_seed") ? static_cast<uint32_t>(cfg->get("random_seed")->as_uint64()) : 0;
    result.run.configuration.use_rdtsc = cfg->has("use_rdtsc") ? cfg->get("use_rdtsc")->as_bool(true) : true;
    result.run.configuration.use_high_resolution_timer = cfg->has("use_high_resolution_timer") ? cfg->get("use_high_resolution_timer")->as_bool(true) : true;
    result.run.configuration.measure_memory = cfg->has("measure_memory") ? cfg->get("measure_memory")->as_bool(true) : true;
    result.run.configuration.cpu_affinity = cfg->has("cpu_affinity") ? static_cast<int>(cfg->get("cpu_affinity")->as_int64(-1)) : -1;

    if (cfg->has("input_sizes") && cfg->get("input_sizes")->is_array()) {
        for (const auto& item : cfg->get("input_sizes")->arr_val) {
            result.run.configuration.input_sizes.push_back(item.as_uint64());
        }
    }
    if (cfg->has("input_cases") && cfg->get("input_cases")->is_array()) {
        for (const auto& item : cfg->get("input_cases")->arr_val) {
            result.run.configuration.input_cases.push_back(item.as_string());
        }
    }

    // 6. Validate input metadata
    const JsonValue* inp = root.get("input");
    if (inp && inp->is_object()) {
        result.run.input.type = inp->has("type") ? inp->get("type")->as_string() : "generated";
        result.run.input.file_path = inp->has("file") ? inp->get("file")->as_string()
                                    : (inp->has("file_path") ? inp->get("file_path")->as_string() : "");
        result.run.input.element_count = inp->has("elements") ? inp->get("elements")->as_uint64()
                                        : (inp->has("element_count") ? inp->get("element_count")->as_uint64() : 0);
        result.run.input.distinct_count = inp->has("distinct") ? inp->get("distinct")->as_uint64()
                                         : (inp->has("distinct_count") ? inp->get("distinct_count")->as_uint64() : 0);
        result.run.input.duplicate_count = inp->has("duplicates") ? inp->get("duplicates")->as_uint64()
                                          : (inp->has("duplicate_count") ? inp->get("duplicate_count")->as_uint64() : 0);
        result.run.input.min_value = inp->has("min") ? static_cast<int>(inp->get("min")->as_int64())
                                    : (inp->has("min_value") ? static_cast<int>(inp->get("min_value")->as_int64()) : 0);
        result.run.input.max_value = inp->has("max") ? static_cast<int>(inp->get("max")->as_int64())
                                    : (inp->has("max_value") ? static_cast<int>(inp->get("max_value")->as_int64()) : 0);
        result.run.input.order_description = inp->has("order") ? inp->get("order")->as_string()
                                            : (inp->has("order_description") ? inp->get("order_description")->as_string() : "");
    }

    // 7. Validate results array
    const JsonValue* res_arr = root.get("results");
    if (!res_arr) {
        res_arr = root.get("summaries"); // backwards compatibility with V3 summaries
    }
    if (!res_arr || !res_arr->is_array()) {
        result.error_message = "Missing or invalid 'results' array";
        return result;
    }

    for (const auto& item : res_arr->arr_val) {
        if (!item.is_object()) {
            result.error_message = "Each element in results array must be an object";
            return result;
        }

        BenchmarkRecord rec;
        rec.algorithm = item.has("algorithm") ? item.get("algorithm")->as_string()
                       : (item.has("key") ? item.get("key")->as_string() : "");
        rec.algorithm_name = item.has("algorithm_name") ? item.get("algorithm_name")->as_string()
                            : (item.has("name") ? item.get("name")->as_string() : rec.algorithm);
        rec.input_type = item.has("input_type") ? item.get("input_type")->as_string()
                        : (item.has("input_case") ? item.get("input_case")->as_string() : "");
        rec.input_size = item.has("input_size") ? item.get("input_size")->as_uint64() : 0;
        rec.verified = item.has("verified") ? item.get("verified")->as_bool(false) : false;

        // Flat or nested time statistics
        if (item.has("time_mean_ns")) {
            rec.time_mean_ns = item.get("time_mean_ns")->as_double();
            rec.time_median_ns = item.has("time_median_ns") ? item.get("time_median_ns")->as_double() : 0.0;
            rec.time_min_ns = item.has("time_min_ns") ? item.get("time_min_ns")->as_double() : 0.0;
            rec.time_max_ns = item.has("time_max_ns") ? item.get("time_max_ns")->as_double() : 0.0;
            rec.time_stddev_ns = item.has("time_stddev_ns") ? item.get("time_stddev_ns")->as_double() : 0.0;
        } else if (item.has("time_ns") && item.get("time_ns")->is_object()) {
            const auto* t = item.get("time_ns");
            rec.time_mean_ns = t->has("mean") ? t->get("mean")->as_double() : 0.0;
            rec.time_median_ns = t->has("median") ? t->get("median")->as_double() : 0.0;
            rec.time_min_ns = t->has("minimum") ? t->get("minimum")->as_double() : 0.0;
            rec.time_max_ns = t->has("maximum") ? t->get("maximum")->as_double() : 0.0;
            rec.time_stddev_ns = t->has("standard_deviation") ? t->get("standard_deviation")->as_double() : 0.0;
        }

        // Flat or nested cycles statistics
        if (item.has("cycles_mean")) {
            rec.cycles_mean = item.get("cycles_mean")->as_double();
            rec.cycles_median = item.has("cycles_median") ? item.get("cycles_median")->as_double() : 0.0;
            rec.cycles_min = item.has("cycles_min") ? item.get("cycles_min")->as_double() : 0.0;
            rec.cycles_max = item.has("cycles_max") ? item.get("cycles_max")->as_double() : 0.0;
            rec.cycles_stddev = item.has("cycles_stddev") ? item.get("cycles_stddev")->as_double() : 0.0;
        } else if (item.has("cycles") && item.get("cycles")->is_object()) {
            const auto* c = item.get("cycles");
            rec.cycles_mean = c->has("mean") ? c->get("mean")->as_double() : 0.0;
            rec.cycles_median = c->has("median") ? c->get("median")->as_double() : 0.0;
            rec.cycles_min = c->has("minimum") ? c->get("minimum")->as_double() : 0.0;
            rec.cycles_max = c->has("maximum") ? c->get("maximum")->as_double() : 0.0;
            rec.cycles_stddev = c->has("standard_deviation") ? c->get("standard_deviation")->as_double() : 0.0;
        }

        // Flat or nested memory statistics
        if (item.has("memory_peak_increase_bytes")) {
            rec.memory_peak_increase_bytes = item.get("memory_peak_increase_bytes")->as_uint64();
            rec.memory_net_change_bytes = item.has("memory_net_change_bytes") ? item.get("memory_net_change_bytes")->as_int64() : 0;
        } else if (item.has("memory_analysis") && item.get("memory_analysis")->is_object()) {
            const auto* m = item.get("memory_analysis");
            rec.memory_peak_increase_bytes = m->has("private_peak_increase_bytes") ? m->get("private_peak_increase_bytes")->as_uint64() : 0;
            rec.memory_net_change_bytes = m->has("private_net_change_bytes") ? m->get("private_net_change_bytes")->as_int64() : 0;
        }

        result.run.results.push_back(std::move(rec));
    }

    result.success = true;
    return result;
}

LoadResult ReportLoader::load_from_json_file(const std::string& filepath) {
    LoadResult result;
    std::ifstream file(filepath);
    if (!file.is_open()) {
        result.error_message = "Unable to open file: " + filepath;
        return result;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return load_from_json_string(buffer.str());
}

std::string ReportLoader::to_json_string(const BenchmarkRun& run) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(6);
    out << "{\n";
    out << "  \"format_version\": \"" << escape_string(run.format_version) << "\",\n";
    out << "  \"run_id\": \"" << escape_string(run.run_id) << "\",\n";
    out << "  \"timestamp\": \"" << escape_string(run.timestamp) << "\",\n";
    out << "  \"benchmark_mode\": \"" << escape_string(run.benchmark_mode) << "\",\n";
    out << "  \"benchmark_category\": \"" << escape_string(run.benchmark_category) << "\",\n";
    if (!run.custom_target_parameter.empty()) {
        out << "  \"custom_target_parameter\": \"" << escape_string(run.custom_target_parameter) << "\",\n";
    }

    // system
    out << "  \"system\": {\n";
    out << "    \"os\": \"" << escape_string(run.system.os) << "\",\n";
    out << "    \"cpu\": \"" << escape_string(run.system.cpu) << "\",\n";
    out << "    \"architecture\": \"" << escape_string(run.system.architecture) << "\",\n";
    out << "    \"physical_cores\": " << run.system.physical_cores << ",\n";
    out << "    \"logical_cpus\": " << run.system.logical_cpus << ",\n";
    out << "    \"compiler\": \"" << escape_string(run.system.compiler) << "\",\n";
    out << "    \"cxx_standard\": \"" << escape_string(run.system.cxx_standard) << "\",\n";
    out << "    \"optimization\": \"" << escape_string(run.system.optimization) << "\"\n";
    out << "  },\n";

    // configuration
    out << "  \"configuration\": {\n";
    out << "    \"warmup_runs\": " << run.configuration.warmup_runs << ",\n";
    out << "    \"iterations\": " << run.configuration.iterations << ",\n";
    out << "    \"random_seed\": " << run.configuration.random_seed << ",\n";
    out << "    \"use_rdtsc\": " << (run.configuration.use_rdtsc ? "true" : "false") << ",\n";
    out << "    \"use_high_resolution_timer\": " << (run.configuration.use_high_resolution_timer ? "true" : "false") << ",\n";
    out << "    \"measure_memory\": " << (run.configuration.measure_memory ? "true" : "false") << ",\n";
    out << "    \"cpu_affinity\": " << run.configuration.cpu_affinity << ",\n";
    out << "    \"input_sizes\": [";
    for (size_t i = 0; i < run.configuration.input_sizes.size(); ++i) {
        out << (i == 0 ? "" : ", ") << run.configuration.input_sizes[i];
    }
    out << "],\n";
    out << "    \"input_cases\": [";
    for (size_t i = 0; i < run.configuration.input_cases.size(); ++i) {
        out << (i == 0 ? "" : ", ") << "\"" << escape_string(run.configuration.input_cases[i]) << "\"";
    }
    out << "]\n";
    out << "  },\n";

    // input
    out << "  \"input\": {\n";
    out << "    \"type\": \"" << escape_string(run.input.type) << "\",\n";
    out << "    \"file_path\": \"" << escape_string(run.input.file_path) << "\",\n";
    out << "    \"element_count\": " << run.input.element_count << ",\n";
    out << "    \"distinct_count\": " << run.input.distinct_count << ",\n";
    out << "    \"duplicate_count\": " << run.input.duplicate_count << ",\n";
    out << "    \"min_value\": " << run.input.min_value << ",\n";
    out << "    \"max_value\": " << run.input.max_value << ",\n";
    out << "    \"order_description\": \"" << escape_string(run.input.order_description) << "\"\n";
    out << "  },\n";

    // results
    out << "  \"results\": [\n";
    for (size_t i = 0; i < run.results.size(); ++i) {
        const auto& r = run.results[i];
        out << "    {\n";
        out << "      \"algorithm\": \"" << escape_string(r.algorithm) << "\",\n";
        out << "      \"algorithm_name\": \"" << escape_string(r.algorithm_name) << "\",\n";
        out << "      \"input_type\": \"" << escape_string(r.input_type) << "\",\n";
        out << "      \"input_size\": " << r.input_size << ",\n";
        out << "      \"verified\": " << (r.verified ? "true" : "false") << ",\n";
        out << "      \"time_mean_ns\": " << r.time_mean_ns << ",\n";
        out << "      \"time_median_ns\": " << r.time_median_ns << ",\n";
        out << "      \"time_min_ns\": " << r.time_min_ns << ",\n";
        out << "      \"time_max_ns\": " << r.time_max_ns << ",\n";
        out << "      \"time_stddev_ns\": " << r.time_stddev_ns << ",\n";
        out << "      \"cycles_mean\": " << r.cycles_mean << ",\n";
        out << "      \"cycles_median\": " << r.cycles_median << ",\n";
        out << "      \"cycles_min\": " << r.cycles_min << ",\n";
        out << "      \"cycles_max\": " << r.cycles_max << ",\n";
        out << "      \"cycles_stddev\": " << r.cycles_stddev << ",\n";
        out << "      \"memory_peak_increase_bytes\": " << r.memory_peak_increase_bytes << ",\n";
        out << "      \"memory_net_change_bytes\": " << r.memory_net_change_bytes << "\n";
        out << "    }" << (i + 1 == run.results.size() ? "" : ",") << "\n";
    }
    out << "  ]\n";
    out << "}\n";
    return out.str();
}

bool ReportLoader::save_to_json_file(const BenchmarkRun& run, const std::string& filepath) {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        return false;
    }
    file << to_json_string(run);
    return static_cast<bool>(file);
}

} // namespace analysis

