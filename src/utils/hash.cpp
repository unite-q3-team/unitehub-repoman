#include "hash.h"
#include <fstream>
#include <vector>
#include <filesystem>
#include <picosha2.h>

namespace utils {

std::string computeFileSha256(const std::string& filePath) {
    try {
        if (!std::filesystem::exists(filePath)) {
            return "";
        }
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) {
            return "";
        }
        std::vector<unsigned char> s(picosha2::k_digest_size);
        picosha2::hash256(file, s.begin(), s.end());
        return picosha2::bytes_to_hex_string(s.begin(), s.end());
    } catch (...) {
        return "";
    }
}

} // namespace utils


