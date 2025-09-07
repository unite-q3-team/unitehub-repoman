#ifndef UTILS_HASH_H
#define UTILS_HASH_H

#include <string>

namespace utils {
    // Returns lowercase hex-encoded SHA-256 of a file. Empty string on error.
    std::string computeFileSha256(const std::string& filePath);
}

#endif // UTILS_HASH_H


