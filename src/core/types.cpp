#include "types.h"

namespace core {

static std::string contentTypeToString(ContentType t) {
    switch (t) {
        case ContentType::PK3: return "pk3";
        case ContentType::CFG: return "cfg";
        case ContentType::EXECUTABLE: return "exe";
    }
    return "pk3";
}

static ContentType contentTypeFromString(const std::string& s) {
    if (s == "pk3") return ContentType::PK3;
    if (s == "cfg") return ContentType::CFG;
    return ContentType::EXECUTABLE;
}

void to_json(nlohmann::json& j, const ContentItem& v) {
    j = nlohmann::json{
        {"id", v.id},
        {"name", v.name},
        {"description", v.description},
        {"author", v.author},
        {"type", contentTypeToString(v.type)},
        {"relative_path", v.relativePath},
        {"sha256", v.sha256},
        {"tags", v.tags},
        {"download_url", v.downloadUrl},
        {"updated_at", v.updatedAt},
        {"file_size", v.fileSizeBytes}
    };
}

void from_json(const nlohmann::json& j, ContentItem& v) {
    j.at("id").get_to(v.id);
    j.at("name").get_to(v.name);
    if (j.contains("description")) j.at("description").get_to(v.description);
    if (j.contains("author")) j.at("author").get_to(v.author);
    {
        std::string t; j.at("type").get_to(t); v.type = contentTypeFromString(t);
    }
    // client/mod moved into tags; keep backward compatibility if present
    if (j.contains("client")) {
        std::string c; j.at("client").get_to(c); if (!c.empty()) v.tags.push_back("client:" + c);
    }
    if (j.contains("mod")) {
        std::string m; j.at("mod").get_to(m); if (!m.empty()) v.tags.push_back("mod:" + m);
    }
    j.at("relative_path").get_to(v.relativePath);
    j.at("sha256").get_to(v.sha256);
    if (j.contains("tags")) j.at("tags").get_to(v.tags);
    if (j.contains("download_url")) j.at("download_url").get_to(v.downloadUrl);
    j.at("updated_at").get_to(v.updatedAt);
    j.at("file_size").get_to(v.fileSizeBytes);
}

void to_json(nlohmann::json& j, const RepoIndex& v) {
    j = nlohmann::json{
        {"version", v.version},
        {"name", v.repositoryName},
        {"description", v.repositoryDescription},
        {"items", v.items}
    };
}

void from_json(const nlohmann::json& j, RepoIndex& v) {
    if (j.contains("version")) j.at("version").get_to(v.version);
    if (j.contains("name")) j.at("name").get_to(v.repositoryName);
    if (j.contains("description")) j.at("description").get_to(v.repositoryDescription);
    if (j.contains("items")) j.at("items").get_to(v.items);
}

} // namespace core


