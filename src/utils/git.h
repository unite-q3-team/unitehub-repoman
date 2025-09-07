#ifndef UTILS_GIT_H
#define UTILS_GIT_H

#include <string>

namespace utils {

class GitManager {
public:
    explicit GitManager(const std::string& repoPath);

    // Initialize git repository
    bool init();

    // Add all files and commit with message
    bool commit(const std::string& message);

    // Check if git repository exists
    bool isInitialized() const;

    // Get current git status
    std::string getStatus() const;

private:
    std::string repoPath;
    
    // Execute git command and return success status
    bool executeGitCommand(const std::string& command, const std::string& workingDir = "") const;
    
    // Execute git command and capture output
    std::string executeGitCommandWithOutput(const std::string& command, const std::string& workingDir = "") const;
};

} // namespace utils

#endif // UTILS_GIT_H
