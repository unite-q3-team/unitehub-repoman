#ifndef UTIL_ZIP_H
#define UTIL_ZIP_H

#include <string>

namespace ziputil {

// Extract a .zip archive into destDir.
// Returns true on success; on failure returns false and sets errorMessage.
bool extractArchive(const std::string& zipPath,
                    const std::string& destDir,
                    std::string& errorMessage);

}

#endif // UTIL_ZIP_H


