#include "path.h"
#include <algorithm>
#include <cctype>

namespace utils {

static bool containsParentRef(const std::string& s) {
    if (s.find("..") == std::string::npos) return false;
    for (size_t i = 0; i + 1 < s.size(); ++i) {
        if (s[i] == '.' && s[i+1] == '.') {
            char left = (i == 0) ? '/' : s[i-1];
            char right = (i + 2 < s.size()) ? s[i+2] : '/';
            if (left == '/' && right == '/') return true;
            if (left == '/' && right == '\0') return true;
        }
    }
    return false;
}

std::string normalizeRelative(const std::string& rawPath) {
    std::string p = rawPath;
    std::replace(p.begin(), p.end(), '\\', '/');
    while (p.rfind("./", 0) == 0) {
        p.erase(0, 2);
    }
    if (!p.empty() && p[0] == '/') {
        p.erase(0, 1);
    }
    return p;
}

bool isSafeRelativePath(const std::string& path) {
    if (path.empty()) return false;
    if (path[0] == '/') return false;
    if (path.size() >= 2 && std::isalpha(static_cast<unsigned char>(path[0])) && path[1] == ':') return false;
    if (path.find('\\') != std::string::npos) return false;
    if (containsParentRef("/" + path + "/")) return false;
    return true;
}

} // namespace utils


