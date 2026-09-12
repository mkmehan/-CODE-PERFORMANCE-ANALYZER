#include "SystemInfo.h"

#include <windows.h>
#include <intrin.h>

#include <array>
#include <cctype>
#include <cstring>
#include <sstream>
#include <vector>

namespace {

std::string trim(std::string value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
        value.pop_back();
    }

    size_t first = 0;
    while (first < value.size() &&
           std::isspace(static_cast<unsigned char>(value[first]))) {
        ++first;
    }
    return value.substr(first);
}

void append_cpuid(std::array<char, 49>& name, const int registers[4], size_t offset) {
    std::memcpy(name.data() + offset, registers, 16);
}

} // namespace

std::string SystemInfo::cpu_name() const {
    int registers[4] = {};
    __cpuid(registers, 0x80000000);
    const unsigned int maximum_extended_leaf = static_cast<unsigned int>(registers[0]);
    if (maximum_extended_leaf < 0x80000004) {
        return "Unknown CPU";
    }

    std::array<char, 49> name{};
    for (unsigned int leaf = 0; leaf < 3; ++leaf) {
        __cpuid(registers, static_cast<int>(0x80000002 + leaf));
        append_cpuid(name, registers, static_cast<size_t>(leaf) * 16);
    }
    return trim(name.data());
}

unsigned int SystemInfo::logical_processors() const {
    const DWORD processors = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    return processors == 0 ? 1U : static_cast<unsigned int>(processors);
}

unsigned int SystemInfo::physical_cores() const {
    DWORD length = 0;
    if (GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &length) ||
        GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        return 1U;
    }

    std::vector<unsigned char> buffer(length);
    auto* info = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.data());
    if (!GetLogicalProcessorInformationEx(
            RelationProcessorCore,
            info,
            &length)) {
        return 1U;
    }

    unsigned int cores = 0;
    DWORD offset = 0;
    while (offset < length) {
        const auto* current = reinterpret_cast<const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(
            buffer.data() + offset);
        if (current->Relationship == RelationProcessorCore) {
            ++cores;
        }
        if (current->Size == 0) {
            break;
        }
        offset += current->Size;
    }
    return cores == 0 ? 1U : cores;
}

std::string SystemInfo::operating_system() const {
    // RtlGetVersion reports the actual NT version without depending on an
    // application compatibility manifest.
    using RtlGetVersionFn = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);

    HMODULE module = GetModuleHandleW(L"ntdll.dll");
    if (module == nullptr) {
        module = LoadLibraryW(L"ntdll.dll");
    }

    if (module != nullptr) {
        const auto rtl_get_version = reinterpret_cast<RtlGetVersionFn>(
            GetProcAddress(module, "RtlGetVersion"));
        if (rtl_get_version != nullptr) {
            RTL_OSVERSIONINFOW version{};
            version.dwOSVersionInfoSize = sizeof(version);
            if (rtl_get_version(&version) == 0) {
                std::ostringstream output;
                output << "Windows " << version.dwMajorVersion << '.'
                       << version.dwMinorVersion << " (Build "
                       << version.dwBuildNumber << ')';
                return output.str();
            }
        }
    }

    return "Windows (version unavailable)";
}

std::string SystemInfo::compiler() const {
#if defined(__clang__)
    return std::string("Clang ") + __clang_version__;
#elif defined(__GNUC__)
    return std::string("GCC ") + __VERSION__;
#elif defined(_MSC_VER)
    return "MSVC " + std::to_string(_MSC_VER);
#else
    return "Unknown compiler";
#endif
}

std::string SystemInfo::architecture() const {
#if defined(_M_X64) || defined(__x86_64__)
    return "x64";
#elif defined(_M_IX86) || defined(__i386__)
    return "x86";
#elif defined(_M_ARM64) || defined(__aarch64__)
    return "ARM64";
#else
    return "Unknown";
#endif
}

std::string SystemInfo::cxx_standard() const {
#if __cplusplus >= 202002L
    return "C++20 or later";
#elif __cplusplus >= 201703L
    return "C++17";
#elif __cplusplus >= 201402L
    return "C++14";
#elif __cplusplus >= 201103L
    return "C++11";
#else
    return "Pre-C++11";
#endif
}

std::string SystemInfo::optimization() const {
#if defined(__OPTIMIZE__)
    return "Enabled (-O2 or equivalent)";
#elif defined(_MSC_VER)
#  if defined(_DEBUG)
    return "Debug build";
#  else
    return "Release/non-debug build (MSVC flag not introspectable)";
#  endif
#elif defined(_DEBUG)
    return "Debug build";
#else
    return "Not reported by compiler";
#endif
}
