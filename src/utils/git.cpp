#include "git.h"
#include "../system/logger.h"
#include <filesystem>
#include <cstdlib>
#include <sstream>

namespace utils {

GitManager::GitManager(const std::string& repoPath) : repoPath(repoPath) {
}

bool GitManager::init() {
    try {
        if (isInitialized()) {
            logger::debug("Git repository already initialized in: " + repoPath);
            return true;
        }

        logger::info("Initializing git repository in: " + repoPath);
        
        // Initialize git repository
        if (!executeGitCommand("init", repoPath)) {
            logger::error("Failed to initialize git repository");
            return false;
        }

        // Create .gitignore to exclude common files
        std::string gitignorePath = repoPath + "/.gitignore";
        std::ofstream gitignore(gitignorePath);
        if (gitignore.is_open()) {
            gitignore << "# RepoMan generated .gitignore\n";
            gitignore << "*.log\n";
            gitignore << "*.tmp\n";
            gitignore << ".DS_Store\n";
            gitignore << "Thumbs.db\n";
            gitignore.close();
            logger::debug("Created .gitignore");
        }

        logger::info("Git repository initialized successfully");
        return true;
    } catch (const std::exception& e) {
        logger::error(std::string("Git init failed: ") + e.what());
        return false;
    }
}

bool GitManager::commit(const std::string& message) {
    try {
        if (!isInitialized()) {
            logger::error("Git repository not initialized");
            return false;
        }

        logger::info("Committing changes: " + message);

        // Add all files
        if (!executeGitCommand("add .", repoPath)) {
            logger::error("Failed to add files to git");
            return false;
        }

        // Check if there are changes to commit
        std::string status = executeGitCommandWithOutput("status --porcelain", repoPath);
        if (status.empty()) {
            logger::info("No changes to commit");
            return true;
        }

        // Commit changes
        std::string commitCmd = "commit -m \"" + message + "\"";
        if (!executeGitCommand(commitCmd, repoPath)) {
            logger::error("Failed to commit changes");
            return false;
        }

        logger::info("Changes committed successfully");
        return true;
    } catch (const std::exception& e) {
        logger::error(std::string("Git commit failed: ") + e.what());
        return false;
    }
}

bool GitManager::isInitialized() const {
    return std::filesystem::exists(repoPath + "/.git");
}

std::string GitManager::getStatus() const {
    if (!isInitialized()) {
        return "Not a git repository";
    }
    return executeGitCommandWithOutput("status --short", repoPath);
}

bool GitManager::executeGitCommand(const std::string& command, const std::string& workingDir) const {
    std::string fullCommand = "git " + command;
    if (!workingDir.empty()) {
        fullCommand = "cd \"" + workingDir + "\" && " + fullCommand;
    }
    
    int result = std::system(fullCommand.c_str());
    return result == 0;
}

std::string GitManager::executeGitCommandWithOutput(const std::string& command, const std::string& workingDir) const {
    std::string fullCommand = "git " + command;
    if (!workingDir.empty()) {
        fullCommand = "cd \"" + workingDir + "\" && " + fullCommand;
    }
    
    // Use popen to capture output
    FILE* pipe = popen(fullCommand.c_str(), "r");
    if (!pipe) {
        return "";
    }
    
    std::string result;
    char buffer[128];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    
    pclose(pipe);
    return result;
}

} // namespace utils
