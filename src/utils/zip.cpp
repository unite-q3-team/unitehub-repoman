#include "zip.h"
#include "../system/logger.h"

#include <filesystem>
#include <string>
#include <vector>
#include <cstdio>

// We will use a fallback approach for now:
// - On Windows: PowerShell Expand-Archive
// - On Unix: unzip
// Later, this can be replaced by libzip when bundling is added to the build.

namespace ziputil {

static bool runCommand(const std::string& cmd, std::string& errorMessage) {
    int rc = std::system(cmd.c_str());
    if (rc != 0) {
        errorMessage = "command failed: exit code " + std::to_string(rc);
        return false;
    }
    return true;
}

bool extractArchive(const std::string& zipPath,
                    const std::string& destDir,
                    std::string& errorMessage) {
    try {
        std::filesystem::create_directories(destDir);
    } catch (const std::exception& e) {
        errorMessage = std::string("failed to create destDir: ") + e.what();
        return false;
    }

#ifdef _WIN32
    auto escapeSingleQuotes = [](const std::string& s) {
        std::string out; out.reserve(s.size());
        for (char c : s) { if (c == '\'') out += "''"; else out.push_back(c); }
        return out;
    };
    std::string script = std::string("$ErrorActionPreference='Stop'; Expand-Archive -LiteralPath '") + escapeSingleQuotes(zipPath) + "' -DestinationPath '" + escapeSingleQuotes(destDir) + "' -Force";
    // Wrap the script in double quotes for -Command
    std::string cmd = std::string("powershell -NoProfile -ExecutionPolicy Bypass -Command \"") + script + "\"";
    logger::debug(std::string("extractArchive: ") + cmd);
    return runCommand(cmd, errorMessage);
#else
    // Prefer Python stdlib zipfile to avoid requiring external 'unzip'
    auto quote = [](const std::string& s){ return std::string("\"") + s + "\""; };
    std::string py = std::string("python3 -c \"import zipfile,sys; zipfile.ZipFile(sys.argv[1]).extractall(sys.argv[2])\" ") + quote(zipPath) + " " + quote(destDir);
    logger::debug(std::string("extractArchive: ") + py);
    std::string errTmp;
    if (runCommand(py, errTmp)) return true;
    std::string py2 = std::string("python -c \"import zipfile,sys; zipfile.ZipFile(sys.argv[1]).extractall(sys.argv[2])\" ") + quote(zipPath) + " " + quote(destDir);
    logger::debug(std::string("extractArchive fallback: ") + py2);
    if (runCommand(py2, errTmp)) return true;
    // Last resort: unzip
    std::string uz = std::string("unzip -q \"") + zipPath + "\" -d \"" + destDir + "\"";
    logger::debug(std::string("extractArchive fallback: ") + uz);
    return runCommand(uz, errorMessage);
#endif
}

}


