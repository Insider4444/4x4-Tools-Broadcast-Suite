#pragma once
#include <windows.h>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace fourbyfour::runtime {
namespace fs = std::filesystem;
inline std::string utf8(const fs::path& path) {
    const auto value = path.u8string(); return {reinterpret_cast<const char*>(value.data()), value.size()};
}
inline fs::path directory(const void* anchor) {
    HMODULE module{};
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(anchor), &module)) throw std::runtime_error("Cannot locate the 4x4Tools module.");
    std::wstring buffer(32768, L'\0');
    const auto length = GetModuleFileNameW(module, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (!length || length >= buffer.size()) throw std::runtime_error("Cannot locate the 4x4Tools directory.");
    buffer.resize(length); return fs::path(buffer).parent_path();
}
inline fs::path root(const void* anchor, const wchar_t* key, const wchar_t* environment, const wchar_t* fallback) {
    std::wstring value(32768, L'\0');
    auto n = GetEnvironmentVariableW(environment, value.data(), static_cast<DWORD>(value.size()));
    if (n && n < value.size()) { value.resize(n); return fs::absolute(value); }
    const auto dir = directory(anchor);
    n = GetPrivateProfileStringW(L"NVIDIA", key, L"", value.data(), static_cast<DWORD>(value.size()), (dir / "4x4Tools.ini").c_str());
    if (n) { value.resize(n); return fs::absolute(value); }
    return dir / "runtime" / fallback;
}
template<class T> T symbol(HMODULE module, const char* name) {
    auto result = reinterpret_cast<T>(GetProcAddress(module, name));
    if (!result) throw std::runtime_error(std::string("Missing NVIDIA API: ") + name);
    return result;
}
struct Libraries {
    std::vector<HMODULE> modules;
    std::vector<DLL_DIRECTORY_COOKIE> directories;
    Libraries() = default;
    Libraries(const Libraries&) = delete;
    Libraries& operator=(const Libraries&) = delete;
    void add(const fs::path& path) {
        auto cookie = AddDllDirectory(path.c_str());
        if (!cookie) throw std::runtime_error("Missing runtime directory: " + utf8(path));
        directories.push_back(cookie);
    }
    HMODULE open(const fs::path& path) {
        if (!fs::is_regular_file(path)) throw std::runtime_error("Missing NVIDIA runtime: " + utf8(path));
        const auto module = LoadLibraryExW(path.c_str(), nullptr,
            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32 | LOAD_LIBRARY_SEARCH_USER_DIRS);
        if (!module) throw std::runtime_error("Cannot load " + utf8(path.filename()) + " (Windows error " + std::to_string(GetLastError()) + ").");
        modules.push_back(module); return module;
    }
    ~Libraries() {
        for (auto i = modules.rbegin(); i != modules.rend(); ++i) FreeLibrary(*i);
        for (auto cookie : directories) RemoveDllDirectory(cookie);
    }
};
}
