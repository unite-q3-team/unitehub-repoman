#ifndef UTILS_PATH_H
#define UTILS_PATH_H

#include <string>

namespace utils {
    // Converts backslashes to forward slashes and trims leading './'
    std::string normalizeRelative(const std::string& rawPath);

    // True if path is safe: not absolute, no ".." segments, uses '/'
    bool isSafeRelativePath(const std::string& path);
}

#endif // UTILS_PATH_H


