#include "liner.h"
#include <iostream>
#include <string>
#include <vector>
#include <cstdio>
#include <fstream>
#ifndef _WIN32
#include <termios.h>
#include <unistd.h>
#endif

namespace utils {

static bool setRawMode(
#ifndef _WIN32
    termios& oldt
#endif
) {
#ifdef _WIN32
    return false;
#else
    if (!isatty(STDIN_FILENO)) return false;
    termios t{};
    if (tcgetattr(STDIN_FILENO, &oldt) != 0) return false;
    t = oldt;
    t.c_lflag &= ~(ICANON | ECHO);
    t.c_cc[VMIN] = 1;
    t.c_cc[VTIME] = 0;
    return tcsetattr(STDIN_FILENO, TCSANOW, &t) == 0;
#endif
}

static void restoreMode(
#ifndef _WIN32
    const termios& oldt
#endif
) {
#ifndef _WIN32
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
#else
    // no-op on Windows
#endif
}

bool readLineInteractive(const std::string& prompt,
                         std::string& outLine,
                         std::vector<std::string>& history,
                         const Completer& completer) {
#ifdef _WIN32
    std::cout << prompt;
    if (!std::getline(std::cin, outLine)) return false;
    if (!outLine.empty()) history.push_back(outLine);
    return true;
#else
    std::cout << prompt << std::flush;
    termios oldt{};
    bool raw = setRawMode(oldt);
    if (!raw) {
        if (!std::getline(std::cin, outLine)) return false;
        if (!outLine.empty()) history.push_back(outLine);
        return true;
    }

    std::string buffer;
    size_t cursorIndex = 0;
    int histIndex = static_cast<int>(history.size());
    while (true) {
        unsigned char c = 0;
        if (read(STDIN_FILENO, &c, 1) != 1) { restoreMode(oldt); return false; }
        if (c == '\n' || c == '\r') {
            std::cout << "\n";
            break;
        } else if (c == 3) { // Ctrl-C
            restoreMode(oldt);
            return false;
        } else if (c == 4) { // Ctrl-D
            restoreMode(oldt);
            return false;
        } else if (c == 127 || c == 8) { // Backspace
            if (cursorIndex > 0) {
                // Compute tail, delete char before cursor
                std::string tail = buffer.substr(cursorIndex);
                // Move cursor left one visually
                std::cout << "\b";
                buffer.erase(cursorIndex - 1, 1);
                cursorIndex--;
                // Rewrite tail and clear last leftover char
                std::cout << tail << ' ';
                // Move cursor back to correct spot
                for (size_t i = 0; i < tail.size() + 1; ++i) std::cout << "\b";
                std::cout << std::flush;
            }
        } else if (c == 9) { // Tab completion
            if (completer) {
                auto suggestions = completer(buffer, cursorIndex);
                if (!suggestions.empty()) {
                    // Find common prefix from current token
                    std::string token;
                    size_t start = cursorIndex;
                    while (start > 0 && !isspace(static_cast<unsigned char>(buffer[start-1]))) --start;
                    token = buffer.substr(start, cursorIndex - start);
                    // If only one, insert full; else show list and insert common prefix
                    std::string common = suggestions[0];
                    for (const auto& s : suggestions) {
                        size_t i = 0;
                        while (i < common.size() && i < s.size() && common[i] == s[i]) ++i;
                        common.resize(i);
                    }
                    if (suggestions.size() == 1 && common.size() >= token.size()) {
                        std::string rest = suggestions[0].substr(token.size());
                        // Insert rest at cursor
                        std::string tail = buffer.substr(cursorIndex);
                        buffer.insert(cursorIndex, rest);
                        cursorIndex += rest.size();
                        std::cout << rest << tail;
                        for (size_t i = 0; i < tail.size(); ++i) std::cout << "\b";
                        std::cout << std::flush;
                    } else if (!common.empty() && common.size() > token.size()) {
                        std::string rest = common.substr(token.size());
                        std::string tail = buffer.substr(cursorIndex);
                        buffer.insert(cursorIndex, rest);
                        cursorIndex += rest.size();
                        std::cout << rest << tail;
                        for (size_t i = 0; i < tail.size(); ++i) std::cout << "\b";
                        std::cout << std::flush;
                    } else {
                        // Print suggestions on new line
                        std::cout << "\n";
                        for (const auto& s : suggestions) std::cout << s << "\n";
                        // Reprint prompt and buffer
                        std::cout << prompt << buffer;
                        // Move cursor back to cursorIndex
                        for (size_t i = buffer.size(); i > cursorIndex; --i) std::cout << "\b";
                        std::cout << std::flush;
                    }
                }
            }
        } else if (c == 27) { // ESC sequence
            unsigned char seq1 = 0, seq2 = 0;
            if (read(STDIN_FILENO, &seq1, 1) != 1 || read(STDIN_FILENO, &seq2, 1) != 1) continue;
            if (seq1 == '[') {
                if (seq2 == 'A') { // Up
                    if (!history.empty() && histIndex > 0) {
                        // Move cursor to end
                        while (cursorIndex < buffer.size()) { std::cout << buffer[cursorIndex++]; }
                        // Clear entire line content
                        for (size_t i = 0; i < buffer.size(); ++i) std::cout << "\b \b";
                        --histIndex;
                        buffer = history[histIndex];
                        std::cout << buffer << std::flush;
                        cursorIndex = buffer.size();
                    }
                } else if (seq2 == 'B') { // Down
                    if (!history.empty() && histIndex < static_cast<int>(history.size()) - 1) {
                        // Move cursor to end and clear
                        while (cursorIndex < buffer.size()) { std::cout << buffer[cursorIndex++]; }
                        for (size_t i = 0; i < buffer.size(); ++i) std::cout << "\b \b";
                        ++histIndex;
                        buffer = history[histIndex];
                        std::cout << buffer << std::flush;
                        cursorIndex = buffer.size();
                    } else if (histIndex == static_cast<int>(history.size()) - 1) {
                        while (cursorIndex < buffer.size()) { std::cout << buffer[cursorIndex++]; }
                        for (size_t i = 0; i < buffer.size(); ++i) std::cout << "\b \b";
                        histIndex = static_cast<int>(history.size());
                        buffer.clear();
                        cursorIndex = 0;
                    }
                } else if (seq2 == 'C') { // Right
                    if (cursorIndex < buffer.size()) {
                        std::cout << buffer[cursorIndex];
                        ++cursorIndex;
                        std::cout << std::flush;
                    }
                } else if (seq2 == 'D') { // Left
                    if (cursorIndex > 0) {
                        std::cout << "\b" << std::flush;
                        --cursorIndex;
                    }
                } else if (seq2 >= '0' && seq2 <= '9') {
                    // Extended sequence like ESC [ 3 ~ (Delete)
                    unsigned char seq3 = 0;
                    if (read(STDIN_FILENO, &seq3, 1) != 1) continue;
                    if (seq2 == '3' && seq3 == '~') { // Delete key
                        if (cursorIndex < buffer.size()) {
                            // Remove char at cursor
                            std::string tail = buffer.substr(cursorIndex + 1);
                            buffer.erase(cursorIndex, 1);
                            std::cout << tail << ' ';
                            for (size_t i = 0; i < tail.size() + 1; ++i) std::cout << "\b";
                            std::cout << std::flush;
                        }
                    } else if ((seq2 == '1' || seq2 == '7') && seq3 == '~') { // Home (common variants)
                        // Move to start: print backspaces equal to cursorIndex
                        for (size_t i = 0; i < cursorIndex; ++i) std::cout << "\b";
                        cursorIndex = 0;
                        std::cout << std::flush;
                    } else if ((seq2 == '4' || seq2 == '8') && seq3 == '~') { // End (common variants)
                        // Move to end: print remaining chars to the right
                        while (cursorIndex < buffer.size()) std::cout << buffer[cursorIndex++];
                        std::cout << std::flush;
                    }
                }
            }
        } else if (c >= 32) { // printable
            char ch = static_cast<char>(c);
            // Tail after cursor
            std::string tail = buffer.substr(cursorIndex);
            buffer.insert(buffer.begin() + static_cast<long>(cursorIndex), ch);
            ++cursorIndex;
            // Print inserted char and redraw tail
            std::cout << ch;
            if (!tail.empty()) {
                std::cout << tail;
                // Move cursor back over tail
                for (size_t i = 0; i < tail.size(); ++i) std::cout << "\b";
            }
            std::cout << std::flush;
        }
    }
    restoreMode(oldt);
    outLine = buffer;
    if (!outLine.empty()) history.push_back(outLine);
    return true;
#endif
}

bool loadHistory(const std::string& path, std::vector<std::string>& history, size_t maxEntries) {
#ifndef _WIN32
    try {
        std::ifstream f(path);
        if (!f.good()) return false;
        std::string line;
        while (std::getline(f, line)) {
            if (!line.empty()) history.push_back(line);
        }
        if (history.size() > maxEntries) {
            history.erase(history.begin(), history.end() - static_cast<long>(maxEntries));
        }
        return true;
    } catch (...) { return false; }
#else
    (void)path; (void)history; (void)maxEntries; return false;
#endif
}

bool saveHistory(const std::string& path, const std::vector<std::string>& history, size_t maxEntries) {
#ifndef _WIN32
    try {
        size_t start = history.size() > maxEntries ? (history.size() - maxEntries) : 0;
        std::ofstream f(path, std::ios::trunc);
        if (!f.is_open()) return false;
        for (size_t i = start; i < history.size(); ++i) f << history[i] << '\n';
        return true;
    } catch (...) { return false; }
#else
    (void)path; (void)history; (void)maxEntries; return false;
#endif
}

}


