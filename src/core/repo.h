#ifndef CORE_REPO_H
#define CORE_REPO_H

#include <string>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>
#include "types.h"

namespace core {

class RepoManager {
public:
    explicit RepoManager(const std::string& rootDir);

    bool init(const std::string& name, const std::string& description);
    bool loadIndex();
    bool saveIndex() const;

    // Remove entries whose files are missing from the repo root.
    // Returns number of removed items.
    std::size_t pruneMissingFiles();

    // Discover files present on disk but missing in index; returns number added
    std::size_t discoverNewFiles();

    // Add file to repo with metadata; returns id
    std::optional<std::string> addFile(const std::string& sourcePath,
                                       ContentType type,
                                       const std::string& relativePath,
                                       const std::string& humanName,
                                       const std::string& description,
                                       const std::string& author,
                                       const std::vector<std::string>& tags,
                                       const std::string& downloadUrl);

    // Remove item by ID (removes from index and deletes file)
    bool removeItem(const std::string& itemId);

    // Rename item by ID
    bool renameItem(const std::string& itemId, const std::string& newName);

    // Move item's file and update its relative path in the index
    // newRelativePath is relative to the repo root; directories will be created as needed
    bool moveItem(const std::string& itemId, const std::string& newRelativePath);

    // Update item metadata fields
    bool updateItemMetadata(const std::string& itemId,
                            const std::string& newName,
                            const std::string& newDescription,
                            const std::string& newAuthor,
                            const std::vector<std::string>& newTags);

    const RepoIndex& index() const { return indexData; }
    RepoIndex& index() { return indexData; }

    std::string getRoot() const { return root; }
    std::string getIndexPath() const;
    // Where files are stored inside repo. Currently the repo root (mirrors install layout).
    std::string getStoragePath() const;

private:
    std::string root;
    RepoIndex indexData;

    // Add an index entry for an existing on-disk file (no copy)
    bool addIndexEntryForExistingFile(const std::string& relativePath,
                                      ContentType type,
                                      const std::string& humanName);
};

}

#endif // CORE_REPO_H


