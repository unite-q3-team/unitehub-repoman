#include "logger.h"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
// Undefine Windows macros that conflict with our enum
#undef ERROR
#else
#include <unistd.h>
#endif

namespace logger {

Logger& Logger::getInstance() {
    static Logger instance;
    static bool initialized = false;
    
    if (!initialized) {
        // Auto-detect color support
        instance.colorsEnabled = Logger::detectColorSupport();
        initialized = true;
    }
    
    return instance;
}

void Logger::setLevel(Level level) {
    currentLevel = level;
}

void Logger::setLogFile(const std::string& filename) {
    logFile = std::make_unique<std::ofstream>(filename, std::ios::app);
    if (!logFile->is_open()) {
        std::cerr << "Warning: Could not open log file: " << filename << std::endl;
        logFile.reset();
    }
}

void Logger::enableConsoleOutput(bool enable) {
    consoleOutput = enable;
}

void Logger::enableColors(bool enable) {
    colorsEnabled = enable;
}

bool Logger::isColorsEnabled() const {
    return colorsEnabled;
}

void Logger::debug(const std::string& message) {
    log(Level::DEBUG, message);
}

void Logger::info(const std::string& message) {
    log(Level::INFO, message);
}

void Logger::warning(const std::string& message) {
    log(Level::WARNING, message);
}

void Logger::error(const std::string& message) {
    log(Level::ERROR, message);
}

void Logger::fatal(const std::string& message) {
    log(Level::FATAL, message);
}

void Logger::log(Level level, const std::string& message) {
    if (level < currentLevel) {
        return;
    }
    
    std::string timestamp = getCurrentTimestamp();
    std::string levelStr = levelToString(level);
    std::string logMessage = "[" + timestamp + "] [" + levelStr + "] " + message;
    
    if (consoleOutput) {
        std::string coloredMessage = logMessage;
        if (colorsEnabled) {
            coloredMessage = getColorCode(level) + logMessage + getResetCode();
        }
        
        if (level >= Level::ERROR) {
            std::cerr << coloredMessage << std::endl;
        } else {
            std::cout << coloredMessage << std::endl;
        }
    }
    
    if (logFile && logFile->is_open()) {
        *logFile << logMessage << std::endl;
        logFile->flush();
    }
}

std::string Logger::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    
    return ss.str();
}

std::string Logger::levelToString(Level level) {
    switch (level) {
        case Level::DEBUG:   return "DEBUG";
        case Level::INFO:    return "INFO ";
        case Level::WARNING: return "WARN ";
        case Level::ERROR:   return "ERROR";
        case Level::FATAL:   return "FATAL";
        default:             return "UNKNOWN";
    }
}

std::string Logger::getColorCode(Level level) {
    switch (level) {
        case Level::DEBUG:   return "\033[36m";  // Cyan
        case Level::INFO:    return "\033[32m";  // Green
        case Level::WARNING: return "\033[33m";  // Yellow
        case Level::ERROR:   return "\033[31m";  // Red
        case Level::FATAL:   return "\033[35m";  // Magenta
        default:             return "\033[0m";   // Reset
    }
}

std::string Logger::getResetCode() {
    return "\033[0m";
}

bool Logger::detectColorSupport() {
#ifdef _WIN32
    // Check if we're running in a modern terminal that supports ANSI
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) {
        return false;
    }
    
    // Check if we're in a modern terminal (Windows Terminal, VS Code, etc.)
    const char* term = getenv("TERM");
    const char* wt_session = getenv("WT_SESSION");
    const char* vs_code = getenv("VSCODE_INJECTION");
    const char* conemu = getenv("ConEmuANSI");
    
    if (term || wt_session || vs_code || conemu) {
        // Enable ANSI escape sequences for modern terminals
        dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(hOut, dwMode);
        return true;
    }
    
    // For cmd.exe, try to enable ANSI support (Windows 10 1511+)
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (SetConsoleMode(hOut, dwMode)) {
        // Test if ANSI actually works by trying to set a color
        // If it works, we'll see the color; if not, we'll see the escape codes
        return true; // Let's be optimistic for Windows 10+
    }
    
    return false;
#else
    // On Unix-like systems, check if stdout is a terminal and supports colors
    if (!isatty(STDOUT_FILENO)) {
        return false;
    }
    
    const char* term = getenv("TERM");
    if (!term) {
        return false;
    }
    
    // Check for color support in TERM
    return (strstr(term, "color") != nullptr || 
            strstr(term, "xterm") != nullptr ||
            strstr(term, "screen") != nullptr ||
            strstr(term, "tmux") != nullptr);
#endif
}

void setLevel(Level level) {
    Logger::getInstance().setLevel(level);
}

void setLogFile(const std::string& filename) {
    Logger::getInstance().setLogFile(filename);
}

void enableConsoleOutput(bool enable) {
    Logger::getInstance().enableConsoleOutput(enable);
}

void enableColors(bool enable) {
    Logger::getInstance().enableColors(enable);
}

void debug(const std::string& message) {
    Logger::getInstance().debug(message);
}

void info(const std::string& message) {
    Logger::getInstance().info(message);
}

void warning(const std::string& message) {
    Logger::getInstance().warning(message);
}

void error(const std::string& message) {
    Logger::getInstance().error(message);
}

void fatal(const std::string& message) {
    Logger::getInstance().fatal(message);
}

}
