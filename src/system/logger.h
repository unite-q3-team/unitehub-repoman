#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <iostream>
#include <fstream>
#include <memory>

namespace logger {
    enum class Level {
        DEBUG = 0,
        INFO = 1,
        WARNING = 2,
        ERROR = 3,
        FATAL = 4
    };

    class Logger {
    public:
        static Logger& getInstance();
        
        void setLevel(Level level);
        void setLogFile(const std::string& filename);
        void enableConsoleOutput(bool enable);
        void enableColors(bool enable);
        bool isColorsEnabled() const;
        
        void debug(const std::string& message);
        void info(const std::string& message);
        void warning(const std::string& message);
        void error(const std::string& message);
        void fatal(const std::string& message);

    private:
        Logger() = default;
        ~Logger() = default;
        Logger(const Logger&) = delete;
        Logger& operator=(const Logger&) = delete;
        
        void log(Level level, const std::string& message);
        std::string getCurrentTimestamp();
        std::string levelToString(Level level);
        std::string getColorCode(Level level);
        std::string getResetCode();
        static bool detectColorSupport();
        
        Level currentLevel = Level::INFO;
        std::unique_ptr<std::ofstream> logFile;
        bool consoleOutput = true;
        bool colorsEnabled = true;
    };

    // Global convenience functions
void setLevel(Level level);
void setLogFile(const std::string& filename);
void enableConsoleOutput(bool enable);
void enableColors(bool enable);
    
    void debug(const std::string& message);
    void info(const std::string& message);
    void warning(const std::string& message);
    void error(const std::string& message);
    void fatal(const std::string& message);
}

#endif // LOGGER_H
