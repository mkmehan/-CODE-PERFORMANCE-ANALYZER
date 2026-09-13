#include "CustomBenchmarkRunner.h"
#include "../analysis/ReportLoader.h"
#include "../analysis/HistoryManager.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace custom {

CustomRunnerExecutionResult CustomBenchmarkRunner::execute(const CustomBenchmarkConfig& config) {
    CustomRunnerExecutionResult res;

    // 1. Compile the custom algorithms into a standalone runner executable
    auto comp_res = CustomBenchmarkCompiler::compile_search_runner(config.algorithms);
    if (!comp_res.success) {
        res.success = false;
        res.error_message = "Compilation Failed:\n" + comp_res.compiler_errors;
        return res;
    }

    // 2. Prepare output JSON path
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    std::string output_json_file = "custom/bin/result_" + std::to_string(now) + ".json";

    // 3. Execute isolated process
    res = run_isolated_process(comp_res.runner_executable_path, config, output_json_file);

    // 4. Archive run into history if successful
    if (res.success && !res.run.run_id.empty()) {
        analysis::HistoryManager history;
        analysis::ReportLoader::save_to_json_file(res.run, "results/history/run_" + res.run.run_id + ".json");
    }

    return res;
}

CustomRunnerExecutionResult CustomBenchmarkRunner::run_isolated_process(
    const std::string& executable_path,
    const CustomBenchmarkConfig& config,
    const std::string& output_json_file
) {
    CustomRunnerExecutionResult res;

#ifdef _WIN32
    STARTUPINFOA si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);

    std::string cmd = "\"" + executable_path + "\" --dataset \"" + config.dataset_path + "\" " +
                      "--target " + std::to_string(config.target_value) + " " +
                      "--iterations " + std::to_string(config.iterations) + " " +
                      "--warmup " + std::to_string(config.warmup_runs) + " " +
                      "--affinity " + std::to_string(config.cpu_affinity) + " " +
                      "--memory " + (config.measure_memory ? "true" : "false") + " " +
                      "--json-output \"" + output_json_file + "\"";

    std::vector<char> cmd_buf(cmd.begin(), cmd.end());
    cmd_buf.push_back('\0');

    if (!CreateProcessA(nullptr, cmd_buf.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        res.success = false;
        res.error_message = "Failed to launch runner process: error code " + std::to_string(GetLastError());
        return res;
    }

    DWORD timeout_ms = static_cast<DWORD>(config.timeout_seconds > 0 ? (config.timeout_seconds * 1000) : 15000);
    DWORD wait_res = WaitForSingleObject(pi.hProcess, timeout_ms);

    if (wait_res == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1001);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        res.success = false;
        res.timed_out = true;
        res.error_message = "Execution timed out (exceeded " + std::to_string(config.timeout_seconds) + "s limit). Algorithm likely contains an infinite loop or deadlock.";
        return res;
    }

    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    res.exit_code = static_cast<int>(exit_code);
    if (exit_code != 0) {
        res.success = false;
        std::string err_desc = "Process exited with error code: " + std::to_string(exit_code);
        if (exit_code == 0xC0000005) err_desc = "Process crashed: Access Violation / Segmentation Fault (0xC0000005)";
        else if (exit_code == 0xC00000FD) err_desc = "Process crashed: Stack Overflow / Infinite Recursion (0xC00000FD)";
        else if (exit_code == 0xC0000094) err_desc = "Process crashed: Integer Division by Zero (0xC0000094)";
        res.error_message = err_desc;
        return res;
    }

#else
    // POSIX fallback
    res.error_message = "POSIX execution not implemented.";
    return res;
#endif

    // Read result JSON
    if (!std::filesystem::exists(output_json_file)) {
        res.success = false;
        res.error_message = "Runner process completed but result output file was not created.";
        return res;
    }

    std::ifstream file(output_json_file);
    if (!file.is_open()) {
        res.success = false;
        res.error_message = "Failed to open runner result JSON file.";
        return res;
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    file.close();
    res.raw_output = ss.str();

    // Check if JSON indicates success or error
    size_t err_pos = res.raw_output.find("\"error_message\":");
    if (err_pos != std::string::npos && res.raw_output.find("\"status\": \"error\"") != std::string::npos) {
        res.success = false;
        res.error_message = "Runner error occurred.";
        return res;
    }

    // Extract inner run JSON if wrapped in { "status": "success", "run": { ... } }
    std::string run_json = res.raw_output;
    size_t run_key_pos = res.raw_output.find("\"run\":");
    if (run_key_pos != std::string::npos) {
        size_t brace_pos = res.raw_output.find('{', run_key_pos);
        size_t last_brace = res.raw_output.rfind('}');
        if (brace_pos != std::string::npos && last_brace != std::string::npos && last_brace > brace_pos) {
            // Find closing brace of "run" object
            size_t second_last_brace = res.raw_output.rfind('}', last_brace - 1);
            if (second_last_brace != std::string::npos && second_last_brace > brace_pos) {
                run_json = res.raw_output.substr(brace_pos, second_last_brace - brace_pos + 1);
            }
        }
    }

    auto load_res = analysis::ReportLoader::load_from_json_string(run_json);
    if (!load_res.success) {
        res.success = false;
        res.error_message = "Failed to parse benchmark results: " + load_res.error_message;
        return res;
    }

    res.success = true;
    res.run = load_res.run;
    return res;
}

} // namespace custom

