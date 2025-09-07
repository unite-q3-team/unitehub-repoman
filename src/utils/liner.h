#ifndef UTILS_LINER_H
#define UTILS_LINER_H

#include <string>
#include <vector>
#include <functional>

namespace utils {

// Cross-platform minimal line editor with history.
// - On Linux/WSL: enables basic arrow-key editing and history navigation
// - On Windows: falls back to std::getline (console usually provides editing)
// Returns false on EOF (Ctrl+D/Ctrl+Z), true otherwise.
using Completer = std::function<std::vector<std::string>(const std::string& buffer, size_t cursorIndex)>;

bool readLineInteractive(const std::string& prompt,
                         std::string& outLine,
                         std::vector<std::string>& history,
                         const Completer& completer = nullptr);

// Persistent history helpers
bool loadHistory(const std::string& path, std::vector<std::string>& history, size_t maxEntries = 1000);
bool saveHistory(const std::string& path, const std::vector<std::string>& history, size_t maxEntries = 1000);

}

#endif // UTILS_LINER_H


