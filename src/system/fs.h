#ifndef FS_H
#define FS_H

#include <string>

namespace fs {
    /**
     * Get the directory path where the current executable is located
     * @return The executable directory path, or empty string on error
     */
    std::string getExecutablePath();
    
    /**
     * Create a directory if it doesn't already exist
     * @param path The directory path to create
     * @return true if directory was created or already exists, false on error
     */
    bool createDirectoryIfNotExists(const std::string& path);
    
    /**
     * Create a file if it doesn't already exist
     * @param filePath The file path to create
     * @param content The content to write to the file (optional)
     * @return true if file was created or already exists, false on error
     */
    bool createFileIfNotExists(const std::string& filePath, const std::string& content = "");
}

#endif // FS_H
