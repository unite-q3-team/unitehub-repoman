#include "config.h"
#include "logger.h"
#include <fstream>

namespace config {

Config& Config::getInstance() {
    static Config instance;
    return instance;
}

bool Config::load(const std::string& configPath) {
    try {
        std::ifstream file(configPath);
        if (!file.is_open()) {
            logger::warning("Config file not found: " + configPath);
            return false;
        }
        
        file >> data;
        isValid = true;
        logger::info("Loaded config from: " + configPath);
        return true;
    } catch (const std::exception& e) {
        logger::error("Failed to load config: " + std::string(e.what()));
        isValid = false;
        return false;
    }
}

bool Config::save(const std::string& configPath) {
    try {
        std::ofstream file(configPath);
        if (!file.is_open()) {
            logger::error("Failed to open config file for writing: " + configPath);
            return false;
        }
        
        file << data.dump(4);
        logger::info("Saved config to: " + configPath);
        return true;
    } catch (const std::exception& e) {
        logger::error("Failed to save config: " + std::string(e.what()));
        return false;
    }
}

void Config::addRepository(const Repository& repo) {
    if (!data.contains("repositories")) {
        data["repositories"] = nlohmann::json::array();
    }
    
    nlohmann::json repoJson;
    repoJson["name"] = repo.name;
    repoJson["path"] = repo.path;
    
    data["repositories"].push_back(repoJson);
    logger::info("Added repository: " + repo.name);
}

void Config::removeRepository(const std::string& name) {
    if (!data.contains("repositories")) {
        return;
    }
    
    auto& repos = data["repositories"];
    for (auto it = repos.begin(); it != repos.end(); ++it) {
        if (it->contains("name") && (*it)["name"] == name) {
            repos.erase(it);
            logger::info("Removed repository: " + name);
            return;
        }
    }
}

std::vector<Repository> Config::getRepositories() const {
    std::vector<Repository> repositories;
    
    if (!data.contains("repositories")) {
        return repositories;
    }
    
    for (const auto& repoJson : data["repositories"]) {
        Repository repo;
        if (repoJson.contains("name")) repo.name = repoJson["name"];
        if (repoJson.contains("path")) repo.path = repoJson["path"];
        
        repositories.push_back(repo);
    }
    
    return repositories;
}

Repository* Config::getRepository(const std::string& name) {
    if (!data.contains("repositories")) {
        return nullptr;
    }
    
    for (auto& repoJson : data["repositories"]) {
        if (repoJson.contains("name") && repoJson["name"] == name) {
            static Repository repo;
            repo.name = repoJson["name"];
            if (repoJson.contains("path")) repo.path = repoJson["path"];
            return &repo;
        }
    }
    
    return nullptr;
}

Settings Config::getSettings() const {
    Settings settings;
    
    // No settings currently used
    
    return settings;
}

void Config::setSettings(const Settings& settings) {
    (void)settings;
}

std::string Config::getCurrentRepo() const {
    if (data.contains("current_repo")) {
        try { return data.at("current_repo"); } catch (...) {}
    }
    return std::string();
}

void Config::setCurrentRepo(const std::string& name) {
    data["current_repo"] = name;
}

void Config::setDefaultSettings() {
    data["settings"] = nlohmann::json::object();
}

void Config::setDefaultRepositories() {
    data["repositories"] = nlohmann::json::array();
}

void Config::setGithubTokenEncrypted(const std::string& enc) {
    data["github"]["token_enc"] = enc;
}

std::string Config::getGithubTokenEncrypted() const {
    if (data.contains("github") && data["github"].contains("token_enc")) {
        try { return data["github"]["token_enc"]; } catch (...) {}
    }
    return std::string();
}

void Config::setGithubUser(const std::string& login) {
    data["github"]["user"] = login;
}

std::string Config::getGithubUser() const {
    if (data.contains("github") && data["github"].contains("user")) {
        try { return data["github"]["user"]; } catch (...) {}
    }
    return std::string();
}

// Global convenience functions
bool loadConfig(const std::string& configPath) {
    return Config::getInstance().load(configPath);
}

bool saveConfig(const std::string& configPath) {
    return Config::getInstance().save(configPath);
}

void addRepository(const Repository& repo) {
    Config::getInstance().addRepository(repo);
}

void removeRepository(const std::string& name) {
    Config::getInstance().removeRepository(name);
}

std::vector<Repository> getRepositories() {
    return Config::getInstance().getRepositories();
}

Repository* getRepository(const std::string& name) {
    return Config::getInstance().getRepository(name);
}

Settings getSettings() {
    return Config::getInstance().getSettings();
}

void setSettings(const Settings& settings) {
    Config::getInstance().setSettings(settings);
}

}
