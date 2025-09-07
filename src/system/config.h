#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace config {
    // Simplified config structures
    struct Repository {
        std::string name;
        std::string path; // absolute path to repo root
    };

    struct Settings {
        // Reserved for future; currently empty to reduce noise
        bool reserved = false;
    };

    class Config {
    public:
        static Config& getInstance();
        
        bool load(const std::string& configPath);
        bool save(const std::string& configPath);
        
        void addRepository(const Repository& repo);
        void removeRepository(const std::string& name);
        std::vector<Repository> getRepositories() const;
        Repository* getRepository(const std::string& name);
        
        Settings getSettings() const;
        void setSettings(const Settings& settings);
        
        // Current repo selection (for CLI convenience)
        std::string getCurrentRepo() const;
        void setCurrentRepo(const std::string& name);
        
        void setDefaultSettings();
        void setDefaultRepositories();

        // GitHub credentials
        void setGithubTokenEncrypted(const std::string& enc);
        std::string getGithubTokenEncrypted() const;
        void setGithubUser(const std::string& login);
        std::string getGithubUser() const;

    private:
        Config() = default;
        ~Config() = default;
        Config(const Config&) = delete;
        Config& operator=(const Config&) = delete;
        
        nlohmann::json data;
        bool isValid = false;
    };

    // Global convenience functions
    bool loadConfig(const std::string& configPath);
    bool saveConfig(const std::string& configPath);
    
    void addRepository(const Repository& repo);
    void removeRepository(const std::string& name);
    std::vector<Repository> getRepositories();
    Repository* getRepository(const std::string& name);
    
    Settings getSettings();
    void setSettings(const Settings& settings);
    
    // Convenience for current repo selection
    inline std::string getCurrentRepo() { return Config::getInstance().getCurrentRepo(); }
    inline void setCurrentRepo(const std::string& name) { Config::getInstance().setCurrentRepo(name); }
}

#endif // CONFIG_H
