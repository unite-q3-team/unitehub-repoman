#ifndef CORE_TYPES_H
#define CORE_TYPES_H

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace core {

enum class ContentType {
    PK3,
    CFG,
    EXECUTABLE
};

struct ContentItem {
    std::string id;                 // stable id
    std::string name;               // human-friendly name
    std::string description;        // optional description
    std::string author;             // optional author
    ContentType type;               // pk3, cfg, exe
    std::string relativePath;       // safe install path relative to root
    std::string sha256;             // file hash
    std::vector<std::string> tags;  // tags/categories
    std::string downloadUrl;        // optional URL
    uint64_t updatedAt;             // unix timestamp
    uint64_t fileSizeBytes;         // size for verification
};

struct RepoIndex {
    std::string version = "1";     // schema version
    std::string repositoryName;
    std::string repositoryDescription;
    std::vector<ContentItem> items;
};

// JSON serialization helpers
NLOHMANN_JSON_SERIALIZE_ENUM(ContentType, {
    {ContentType::PK3, "pk3"},
    {ContentType::CFG, "cfg"},
    {ContentType::EXECUTABLE, "exe"}
})

void to_json(nlohmann::json& j, const ContentItem& v);
void from_json(const nlohmann::json& j, ContentItem& v);
void to_json(nlohmann::json& j, const RepoIndex& v);
void from_json(const nlohmann::json& j, RepoIndex& v);

} // namespace core

#endif // CORE_TYPES_H


