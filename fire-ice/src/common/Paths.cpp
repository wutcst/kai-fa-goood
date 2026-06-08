#include "Paths.hpp"

#include <filesystem>
#include <fstream>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace fireice {

namespace {

std::filesystem::path executableDirectory() {
#ifdef _WIN32
    char buffer[MAX_PATH]{};
    const DWORD length = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    if (length == 0 || length == MAX_PATH) {
        return std::filesystem::current_path();
    }
    return std::filesystem::path(buffer).parent_path();
#else
    return std::filesystem::current_path();
#endif
}

bool fileReadable(const std::filesystem::path& path) {
    std::ifstream file(path);
    return file.good();
}

} // namespace

std::string resolveAssetPath(const std::string& path) {
    const std::filesystem::path requested(path);
    if (requested.is_absolute() && fileReadable(requested)) {
        return requested.string();
    }

    if (fileReadable(requested)) {
        return std::filesystem::absolute(requested).string();
    }

    const std::filesystem::path besideExe = executableDirectory() / requested;
    if (fileReadable(besideExe)) {
        return besideExe.string();
    }

    const std::filesystem::path besideExeFile = executableDirectory() / requested.filename();
    if (fileReadable(besideExeFile)) {
        return besideExeFile.string();
    }

    return path;
}

} // namespace fireice
