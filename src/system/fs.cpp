#include "fs.h"
#include "logger.h"
#include <filesystem>
#include <fstream>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <limits.h>
#endif

namespace fs {

std::string getExecutablePath() {
#ifdef _WIN32
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    std::filesystem::path exePath(path);
    return exePath.parent_path().string();
#else
    char path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (len != -1) {
        path[len] = '\0';
        std::filesystem::path exePath(path);
        return exePath.parent_path().string();
    }
    return "";
#endif
}

bool createDirectoryIfNotExists(const std::string& path) {
    try {
        if (!std::filesystem::exists(path)) {
            std::filesystem::create_directories(path);
            logger::info("Created directory: " + path);
            return true;
        }
        logger::debug("Directory already exists: " + path);
        return false;
    } catch (const std::exception& e) {
        logger::error("Failed to create directory " + path + ": " + e.what());
        return false;
    }
}

bool createFileIfNotExists(const std::string& filePath, const std::string& content) {
    try {
        if (!std::filesystem::exists(filePath)) {
            std::ofstream file(filePath);
            if (file.is_open()) {
                file << content;
                file.close();
                logger::info("Created file: " + filePath);
                return true;
            } else {
                logger::error("Failed to create file: " + filePath);
                return false;
            }
        }
        logger::debug("File already exists: " + filePath);
        return false;
    } catch (const std::exception& e) {
        logger::error("Exception creating file " + filePath + ": " + e.what());
        return false;
    }
}

} // namespace fs
