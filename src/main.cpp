#include <iostream>
#include <string>
#include "system/fs.h"
#include "system/logger.h"
#include "system/config.h"
#include "cli/cli.h"

int main(int argc, char** argv) {
    // Default to INFO; can be raised to DEBUG via --verbose
    logger::setLevel(logger::Level::INFO);
    // Early lightweight scan for --verbose to enable debug before any logs
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--verbose") {
            logger::setLevel(logger::Level::DEBUG);
            break;
        }
    }
    logger::setLogFile("repoman.log");
    // Colors are auto-detected based on terminal support
    
    logger::info("Repoman CLI - Starting initialization");
    logger::debug("Debug mode enabled - showing detailed information");
    logger::debug("Color support: " + std::string(logger::Logger::getInstance().isColorsEnabled() ? "enabled" : "disabled"));
    logger::info("Checking for required directories and files...");
 
    std::string exeDir = fs::getExecutablePath();
    if (exeDir.empty()) {
        logger::fatal("Could not determine executable path");
        return 1;
    }
    
    logger::info("Executable directory: " + exeDir);
    
    std::string reposPath = exeDir + "/repos";
    logger::debug("Checking repos directory: " + reposPath);
    fs::createDirectoryIfNotExists(reposPath);
    
    std::string configPath = exeDir + "/config.json";
    logger::debug("Checking config file: " + configPath);
    
    if (!config::loadConfig(configPath)) {
        logger::info("Creating default configuration");
        config::Config::getInstance().setDefaultSettings();
        config::Config::getInstance().setDefaultRepositories();
        config::saveConfig(configPath);
    } else {
        logger::info("Loaded existing configuration");
    }
    
    logger::info("Setup complete!");
    
    // Delegate to CLI handler (argparse will show help if no args)
    return cli::runCommand(argc, argv);
}
