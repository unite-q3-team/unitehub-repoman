#include "repo.h"
#include "types.h"
#include "../system/logger.h"
#include "../system/fs.h"
#include "../utils/hash.h"
#include "../utils/path.h"
#include "../utils/git.h"

#include <filesystem>
#include <fstream>
#include <random>
#include <unordered_set>
#include <chrono>

namespace core {

static std::string generateId() {
    static std::mt19937_64 rng{std::random_device{}()};
    uint64_t a = rng();
    uint64_t b = rng();
    char buf[33];
    std::snprintf(buf, sizeof(buf), "%016llx%016llx",
                  static_cast<unsigned long long>(a),
                  static_cast<unsigned long long>(b));
    return std::string(buf);
}

static bool isAsciiString(const std::string& text) {
    for (unsigned char ch : text) {
        if (ch > 0x7F) return false;
    }
    return true;
}

RepoManager::RepoManager(const std::string& rootDir) : root(rootDir) {}

std::string RepoManager::getIndexPath() const {
    return root + "/index.json";
}

std::string RepoManager::getStoragePath() const {
    // Store files at the repo root to mirror the Quake 3 layout (e.g., baseq3/*, osp/*).
    return root;
}

bool RepoManager::init(const std::string& name, const std::string& description) {
    try {
        std::filesystem::create_directories(root);
        std::filesystem::create_directories(getStoragePath());
        indexData.repositoryName = name;
        indexData.repositoryDescription = description;
        
        if (!saveIndex()) {
            return false;
        }

        // Initialize git repository
        utils::GitManager git(root);
        if (!git.init()) {
            logger::warning("Failed to initialize git repository, continuing without git");
        } else {
            // Create initial commit
            std::string commitMessage = "Initial commit: " + name;
            if (!description.empty()) {
                commitMessage += " - " + description;
            }
            if (!git.commit(commitMessage)) {
                logger::warning("Failed to create initial commit, continuing without git");
            } else {
                logger::info("Git repository initialized with initial commit");
            }
        }

        return true;
    } catch (const std::exception& e) {
        logger::error(std::string("Repo init failed: ") + e.what());
        return false;
    }
}

bool RepoManager::loadIndex() {
    try {
        std::ifstream in(getIndexPath());
        if (!in.is_open()) return false;
        nlohmann::json j; in >> j; indexData = j.get<RepoIndex>();
        return true;
    } catch (const std::exception& e) {
        logger::error(std::string("Load index failed: ") + e.what());
        return false;
    }
}

bool RepoManager::saveIndex() const {
    try {
        std::ofstream out(getIndexPath());
        if (!out.is_open()) return false;
        nlohmann::json j = indexData;
        out << j.dump(2);
        return true;
    } catch (const std::exception& e) {
        logger::error(std::string("Save index failed: ") + e.what());
        return false;
    }
}

std::size_t RepoManager::pruneMissingFiles() {
    std::size_t removed = 0;
    std::vector<ContentItem> kept;
    kept.reserve(indexData.items.size());
    for (const auto& it : indexData.items) {
        std::filesystem::path p = std::filesystem::path(getStoragePath()) / it.relativePath;
        if (std::filesystem::exists(p)) {
            kept.push_back(it);
        } else {
            ++removed;
        }
    }
    if (removed > 0) {
        indexData.items.swap(kept);
        saveIndex();
    }
    return removed;
}

std::size_t RepoManager::discoverNewFiles() {
    std::size_t added = 0;
    // Build a set of known relative paths
    std::unordered_set<std::string> known;
    known.reserve(indexData.items.size() * 2);
    for (const auto& it : indexData.items) known.insert(it.relativePath);

    std::error_code ec;
    for (std::filesystem::recursive_directory_iterator it(getStoragePath(), ec), end; it != end && !ec; it.increment(ec)) {
        if (!it->is_regular_file()) continue;
        auto abs = it->path();
        // Compute relative path from repo root
        std::filesystem::path relPath = std::filesystem::relative(abs, getStoragePath(), ec);
        if (ec) continue;

        // Use UTF-8 with generic separators '/'
        std::string rel = relPath.generic_u8string();
        // Skip repo index itself and Git internals
        if (rel == "index.json") continue;
        if (rel.size() >= 4 && rel.substr(0, 4) == ".git") continue;
        if (known.find(rel) != known.end()) continue;

        // Infer content type from extension
        std::string ext = abs.extension().u8string();
        for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        ContentType type = ContentType::PK3;
        if (ext == ".cfg") type = ContentType::CFG;
        else if (ext == ".exe") type = ContentType::EXECUTABLE;

        // Derive human name from filename (without extension)
        std::string stem = abs.stem().u8string();
        logger::debug("discovery: candidate '" + rel + "'");
        if (!isAsciiString(rel)) {
            logger::warning("discovery: skipped non-ASCII path '" + rel + "'");
            continue;
        }
        if (addIndexEntryForExistingFile(rel, type, stem)) {
            logger::info("discovery: added '" + rel + "'");
            known.insert(rel);
            ++added;
        }
    }
    if (added > 0) saveIndex();
    return added;
}

bool RepoManager::addIndexEntryForExistingFile(const std::string& relativePath,
                                               ContentType type,
                                               const std::string& humanName) {
    try {
        if (!isAsciiString(relativePath)) {
            logger::warning("Index add skipped: non-ASCII path '" + relativePath + "'");
            return false;
        }
        std::filesystem::path full = std::filesystem::path(getStoragePath()) / std::filesystem::u8path(relativePath);
        if (!std::filesystem::exists(full)) return false;

        std::string sha = utils::computeFileSha256(full.u8string());
        uint64_t size = std::filesystem::file_size(full);

        ContentItem item;
        item.id = generateId();
        item.name = humanName;
        item.description = "";
        item.author = "";
        item.type = type;
        item.relativePath = relativePath;
        item.sha256 = sha;
        item.tags = {};
        item.downloadUrl = "";
        item.updatedAt = static_cast<uint64_t>(
            std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
        item.fileSizeBytes = size;

        indexData.items.push_back(item);
        return true;
    } catch (const std::exception& e) {
        logger::error(std::string("Index discovery add failed: ") + e.what());
        return false;
    }
}

std::optional<std::string> RepoManager::addFile(const std::string& sourcePath,
                                       ContentType type,
                                       const std::string& relativePath,
                                       const std::string& humanName,
                                       const std::string& description,
                                       const std::string& author,
                                       const std::vector<std::string>& tags,
                                       const std::string& downloadUrl) {
    std::string rel = utils::normalizeRelative(relativePath);
    if (!utils::isSafeRelativePath(rel)) {
        logger::error("Unsafe relative path: " + relativePath);
        return std::nullopt;
    }
    // Enforce ASCII-only paths and filenames
    if (!isAsciiString(rel)) {
        logger::error("Non-ASCII characters are not allowed in paths: '" + rel + "'");
        return std::nullopt;
    }
    {
        // Validate filename segment
        auto pos = rel.find_last_of('/');
        std::string filename = (pos == std::string::npos) ? rel : rel.substr(pos + 1);
        if (!isAsciiString(filename)) {
            logger::error("Non-ASCII characters are not allowed in file names: '" + filename + "'");
            return std::nullopt;
        }
    }

    try {
        std::filesystem::create_directories(getStoragePath());
        // Copy file into storage under normalized relative path
        std::filesystem::path dest = std::filesystem::path(getStoragePath()) / std::filesystem::u8path(rel);
        std::filesystem::create_directories(dest.parent_path());
        // Normalize Windows-style source path with backslashes preserved by parser
        std::filesystem::path srcPath(sourcePath);
        std::filesystem::copy_file(srcPath, dest, std::filesystem::copy_options::overwrite_existing);

        std::string sha = utils::computeFileSha256(dest.u8string());
        uint64_t size = std::filesystem::file_size(dest);

        ContentItem item;
        item.id = generateId();
        item.name = humanName;
        item.description = description;
        item.author = author;
        item.type = type;
        item.relativePath = rel;
        item.sha256 = sha;
        item.tags = tags;
        item.downloadUrl = downloadUrl;
        item.updatedAt = static_cast<uint64_t>(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
        item.fileSizeBytes = size;

        indexData.items.push_back(item);
        if (!saveIndex()) return std::nullopt;
        return item.id;
    } catch (const std::exception& e) {
        logger::error(std::string("Add file failed: ") + e.what());
        return std::nullopt;
    }
}

bool RepoManager::removeItem(const std::string& itemId) {
    try {
        // Find the item in the index
        auto it = std::find_if(indexData.items.begin(), indexData.items.end(),
                              [&itemId](const ContentItem& item) { return item.id == itemId; });
        
        if (it == indexData.items.end()) {
            logger::error("Item not found: " + itemId);
            return false;
        }

        // Delete the physical file
        std::string filePath = root + "/" + it->relativePath;
        if (std::filesystem::exists(filePath)) {
            std::filesystem::remove(filePath);
            logger::debug("Deleted file: " + filePath);
        }

        // Remove from index
        indexData.items.erase(it);
        
        // Save updated index
        if (!saveIndex()) {
            logger::error("Failed to save index after removing item");
            return false;
        }

        logger::info("Removed item: " + itemId);
        return true;
    } catch (const std::exception& e) {
        logger::error(std::string("Remove item failed: ") + e.what());
        return false;
    }
}

bool RepoManager::renameItem(const std::string& itemId, const std::string& newName) {
    try {
        // Find the item in the index
        auto it = std::find_if(indexData.items.begin(), indexData.items.end(),
                              [&itemId](const ContentItem& item) { return item.id == itemId; });
        
        if (it == indexData.items.end()) {
            logger::error("Item not found: " + itemId);
            return false;
        }

        // Update the name
        std::string oldName = it->name;
        it->name = newName;
        
        // Save updated index
        if (!saveIndex()) {
            logger::error("Failed to save index after renaming item");
            return false;
        }

        logger::info("Renamed item " + itemId + " from '" + oldName + "' to '" + newName + "'");
        return true;
    } catch (const std::exception& e) {
        logger::error(std::string("Rename item failed: ") + e.what());
        return false;
    }
}

} // namespace core


