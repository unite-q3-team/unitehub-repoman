#include "cli.h"
#include "../system/logger.h"
#include "../system/fs.h"
#include "../core/repo.h"
#include "../system/config.h"
#include "../utils/path.h"
#include "../utils/hash.h"
#include <argparse/argparse.hpp>
#include "../utils/liner.h"
#include "../utils/zip.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <sstream>
#include <filesystem>
#include <fstream>
#include <cstdlib>
#ifdef _WIN32
#include <windows.h>
#endif

namespace cli {

// Simple tokenizer: splits by spaces unless inside double quotes. Preserves backslashes
// and does NOT interpret escape sequences like \r or \n.
static std::vector<std::string> parseArgs(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    bool in_quotes = false;
    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (c == '"') {
            in_quotes = !in_quotes;
            continue;
        }
        if (!in_quotes && std::isspace(static_cast<unsigned char>(c))) {
            if (!cur.empty()) { out.push_back(cur); cur.clear(); }
            continue;
        }
        // Preserve backslashes; special-case \" inside quotes to include a quote
        if (c == '\\' && in_quotes && i + 1 < line.size() && line[i+1] == '"') {
            cur.push_back('"');
            ++i;
            continue;
        }
        cur.push_back(c);
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}


int runCommand(int argc, char** argv) {
    // Pre-scan for --verbose anywhere and strip it so subcommands don't see it as unknown
    bool verboseDetected = false;
    std::vector<char*> filteredArgv;
    filteredArgv.reserve(static_cast<size_t>(argc));
    if (argc > 0) filteredArgv.push_back(argv[0]);
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--verbose") { verboseDetected = true; continue; }
        filteredArgv.push_back(argv[i]);
    }
    int filteredArgc = static_cast<int>(filteredArgv.size());

    argparse::ArgumentParser program("repoman-cli");
    program.add_description("RepoMan - manage Quake 3 content repositories");
    program.add_epilog(
        "IMPORTANT: ASCII-only file and path names are required.\n"
        "Unicode (e.g., Cyrillic) in file names or directories is not supported\n"
        "and may result in corrupted paths or indexing failures.\n");

    program.add_argument("--verbose")
        .help("enable verbose (debug) logging")
        .default_value(false)
        .implicit_value(true);

    program.add_argument("--repo")
        .help("target repository name for this command")
        .default_value(std::string(""));

    // Subcommands
    argparse::ArgumentParser init_parser("init");
    init_parser.add_argument("name").help("repository name").default_value(std::string("MyRepo"));
    init_parser.add_argument("desc").help("optional description").default_value(std::string(""));
    program.add_subparser(init_parser);

    argparse::ArgumentParser use_parser("use");
    use_parser.add_argument("name").help("repository name");
    program.add_subparser(use_parser);

    argparse::ArgumentParser add_parser("add");
    add_parser.add_argument("src").help("path to local file (.pk3/.cfg/.exe)");
    add_parser.add_argument("type").help("pk3|cfg|exe").choices("pk3", "cfg", "exe");
    add_parser.add_argument("rel").help("install path relative to repo root (e.g., baseq3/mymap.pk3)");
    add_parser.add_argument("name").help("human-friendly name for the pack/file");
    add_parser.add_argument("--author").help("optional author name").default_value(std::string(""));
    add_parser.add_argument("--desc").help("optional description").default_value(std::string(""));
    add_parser.add_argument("--tag").append().help("add metadata tag (e.g., client:ioq3, mod:osp, category:maps)");
    program.add_subparser(add_parser);

    argparse::ArgumentParser list_parser("list");
    program.add_subparser(list_parser);

    argparse::ArgumentParser index_parser("index");
    program.add_subparser(index_parser);

    // Verify repo
    argparse::ArgumentParser verify_parser("verify");
    program.add_subparser(verify_parser);

    argparse::ArgumentParser remove_parser("remove");
    remove_parser.add_argument("id").help("item ID to remove");
    program.add_subparser(remove_parser);

    argparse::ArgumentParser rename_parser("rename");
    rename_parser.add_argument("id").help("item ID to rename");
    rename_parser.add_argument("new_name").help("new name for the item");
    program.add_subparser(rename_parser);

    argparse::ArgumentParser repl_parser("repl");
    program.add_subparser(repl_parser);

    argparse::ArgumentParser list_repos_parser("list-repos");
    program.add_subparser(list_repos_parser);

    argparse::ArgumentParser delete_repo_parser("delete-repo");
    delete_repo_parser.add_argument("name").help("repository name to delete");
    delete_repo_parser.add_argument("--force").help("skip confirmation prompt").default_value(false).implicit_value(true);
    program.add_subparser(delete_repo_parser);

    argparse::ArgumentParser rename_repo_parser("rename-repo");
    rename_repo_parser.add_argument("old_name").help("current repository name");
    rename_repo_parser.add_argument("new_name").help("new repository name");
    program.add_subparser(rename_repo_parser);

    // GitHub commands
    argparse::ArgumentParser gh_login_parser("gh-login");
    program.add_subparser(gh_login_parser);

    argparse::ArgumentParser gh_list_parser("gh-list");
    program.add_subparser(gh_list_parser);

    // Token check
    argparse::ArgumentParser gh_token_check_parser("gh-token-check");
    program.add_subparser(gh_token_check_parser);

    // Pull latest compatible repo state
    argparse::ArgumentParser gh_pull_parser("gh-pull");
    gh_pull_parser.add_argument("--remote").help("owner/repo to pull (default: current repo)").default_value(std::string(""));
    gh_pull_parser.add_argument("--branch").help("branch to pull").default_value(std::string("main"));
    program.add_subparser(gh_pull_parser);

    argparse::ArgumentParser gh_clone_parser("gh-clone");
    gh_clone_parser.add_argument("remote").help("owner/repo to clone");
    gh_clone_parser.add_argument("--branch").help("branch to use").default_value(std::string("main"));
    gh_clone_parser.add_argument("--name").help("local repo name (default repo)").default_value(std::string(""));
    gh_clone_parser.add_argument("--set-current").help("set as current repo after clone").default_value(false).implicit_value(true);
    program.add_subparser(gh_clone_parser);

    argparse::ArgumentParser gh_push_parser("gh-push");
    gh_push_parser.add_argument("--remote").help("owner/repo destination").default_value(std::string(""));
    gh_push_parser.add_argument("--branch").help("branch to push").default_value(std::string("main"));
    gh_push_parser.add_argument("--create").help("create repo if missing (user repos)").default_value(false).implicit_value(true);
    gh_push_parser.add_argument("--private").help("create as private when --create").default_value(false).implicit_value(true);
    gh_push_parser.add_argument("--force").help("force push (overwrite remote history)").default_value(false).implicit_value(true);
    program.add_subparser(gh_push_parser);

    argparse::ArgumentParser gh_delete_parser("gh-delete");
    gh_delete_parser.add_argument("remote").help("owner/repo to delete");
    gh_delete_parser.add_argument("--force").help("skip confirmation").default_value(false).implicit_value(true);
    program.add_subparser(gh_delete_parser);

    argparse::ArgumentParser gh_visibility_parser("gh-visibility");
    gh_visibility_parser.add_argument("remote").help("owner/repo to update");
    gh_visibility_parser.add_argument("visibility").help("public|private").choices("public","private");
    program.add_subparser(gh_visibility_parser);

    try {
        program.parse_args(filteredArgc, filteredArgv.data());
    } catch (const std::exception& e) {
        logger::error(e.what());
        std::cerr << program;
        return 1;
    }

    // Apply verbosity after parsing (also caught early in main)
    if (verboseDetected) {
        logger::setLevel(logger::Level::DEBUG);
        logger::debug("Verbose mode enabled");
    }

    std::string exeDir = fs::getExecutablePath();
    auto getConfigPath = [&]() { return exeDir + "/config.json"; };
    // Load config once per command invocation
    config::loadConfig(getConfigPath());
    auto getSelectedRepoName = [&](const std::string& fromFlag) -> std::string {
        if (!fromFlag.empty()) return fromFlag;
        // config already loaded
        return config::getCurrentRepo();
    };

    if (program.is_subcommand_used("init")) {
        std::string name = init_parser.get<std::string>("name");
        std::string desc = init_parser.get<std::string>("desc");
        std::string repoRoot = exeDir + "/repos/" + name;
        fs::createDirectoryIfNotExists(exeDir + "/repos");
        core::RepoManager repo(repoRoot);
        if (!repo.init(name, desc)) { logger::error("init failed"); return 1; }
        // Auto-select this repo (equivalent to `use <name>`)
        // config already loaded
        config::setCurrentRepo(name);
        if (!config::saveConfig(getConfigPath())) { logger::error("Failed to save config"); return 1; }
        logger::info("Initialized repo: " + name);
        logger::info("Selected repo: " + name);
        return 0;
    } else if (program.is_subcommand_used("use")) {
        std::string name = use_parser.get<std::string>("name");
        std::string repoRoot = exeDir + "/repos/" + name;
        if (!std::filesystem::exists(repoRoot)) { logger::error("Repo not found: " + name); return 1; }
        // config already loaded
        config::setCurrentRepo(name);
        if (!config::saveConfig(getConfigPath())) { logger::error("Failed to save config"); return 1; }
        logger::info("Selected repo: " + name);
        return 0;
    } else if (program.is_subcommand_used("add")) {
        std::string repoFlag = program.get<std::string>("--repo");
        std::string selectedRepoName = getSelectedRepoName(repoFlag);
        if (selectedRepoName.empty()) { logger::error("No repo selected. Use 'use <name>' or add --repo <name>."); return 1; }
        std::string src = add_parser.get<std::string>("src");
        std::string typeStr = add_parser.get<std::string>("type");
        std::string rel = add_parser.get<std::string>("rel");
        std::string name = add_parser.get<std::string>("name");
        std::string author = add_parser.get<std::string>("--author");
        std::string desc = add_parser.get<std::string>("--desc");
        std::vector<std::string> tags;
        if (add_parser.is_used("--tag")) tags = add_parser.get<std::vector<std::string>>("--tag");

        core::ContentType t = core::ContentType::PK3;
        if (typeStr == "cfg") t = core::ContentType::CFG;
        else if (typeStr == "exe") t = core::ContentType::EXECUTABLE;

        std::filesystem::path sp(src);
        std::string filename = sp.filename().string();
        if (rel == "/" || rel == "." || rel == "-") {
            rel = filename;
        }
        rel = utils::normalizeRelative(rel);
        bool endsWithSlash = !rel.empty() && rel.back() == '/';
        auto lastSlash = rel.find_last_of('/');
        std::string lastComponent = (lastSlash == std::string::npos) ? rel : rel.substr(lastSlash + 1);
        bool hasDotInLast = lastComponent.find('.') != std::string::npos;
        if (rel.empty() || endsWithSlash || !hasDotInLast) {
            if (!rel.empty() && rel.back() != '/') rel += '/';
            rel += filename;
        }

        std::string repoRoot = exeDir + "/repos/" + selectedRepoName;
        core::RepoManager repo(repoRoot);
        repo.loadIndex();
        auto id = repo.addFile(src, t, rel, name, desc, author, tags, "");
        if (!id) { logger::error("add failed"); return 1; }
        logger::info("Added item id=" + *id);
        return 0;
    } else if (program.is_subcommand_used("list")) {
        std::string repoFlag = program.get<std::string>("--repo");
        std::string selectedRepoName = getSelectedRepoName(repoFlag);
        if (selectedRepoName.empty()) { logger::error("No repo selected. Use 'use <name>' or add --repo <name>."); return 1; }
        std::string repoRoot = exeDir + "/repos/" + selectedRepoName;
        core::RepoManager repo(repoRoot);
        if (!repo.loadIndex()) { logger::warning("no index.json in repo '" + selectedRepoName + "'"); return 0; }
        for (const auto& it : repo.index().items) {
            std::cout << it.id << "  " << it.name << "  (" << it.relativePath << ")\n";
        }
        return 0;
    } else if (program.is_subcommand_used("index")) {
    } else if (program.is_subcommand_used("verify")) {
        std::string repoFlag = program.get<std::string>("--repo");
        std::string selectedRepoName = getSelectedRepoName(repoFlag);
        if (selectedRepoName.empty()) { logger::error("No repo selected. Use 'use <name>' or add --repo <name>."); return 1; }
        std::string repoRoot = exeDir + "/repos/" + selectedRepoName;
        core::RepoManager repo(repoRoot);
        if (!repo.loadIndex()) { logger::error("Failed to load repository index"); return 1; }
        const auto& items = repo.index().items;
        // Check existence and size/hash mismatches; report duplicates by relativePath or id
        std::unordered_map<std::string, int> pathCount;
        std::unordered_map<std::string, int> idCount;
        size_t missing = 0, hashMismatch = 0, dupPaths = 0, dupIds = 0;
        for (const auto& it : items) {
            pathCount[it.relativePath]++;
            idCount[it.id]++;
            std::string full = repoRoot + "/" + it.relativePath;
            if (!std::filesystem::exists(full)) { ++missing; std::cout << "MISSING: " << it.relativePath << " (" << it.name << ")\n"; continue; }
            if (!it.sha256.empty()) {
                std::string got = utils::computeFileSha256(full);
                if (!got.empty() && got != it.sha256) { ++hashMismatch; std::cout << "HASH MISMATCH: " << it.relativePath << " expected=" << it.sha256 << " got=" << got << "\n"; }
            }
        }
        for (const auto& [p,c] : pathCount) if (c > 1) { dupPaths += c - 1; std::cout << "DUP PATH: " << p << " x" << c << "\n"; }
        for (const auto& [p,c] : idCount) if (c > 1) { dupIds += c - 1; std::cout << "DUP ID: " << p << " x" << c << "\n"; }
        std::cout << "Verify summary: missing=" << missing << ", hashMismatch=" << hashMismatch << ", dupPaths=" << dupPaths << ", dupIds=" << dupIds << "\n";
        return 0;
    } else if (program.is_subcommand_used("remove")) {
        std::string repoFlag = program.get<std::string>("--repo");
        std::string selectedRepoName = getSelectedRepoName(repoFlag);
        if (selectedRepoName.empty()) { logger::error("No repo selected. Use 'use <name>' or add --repo <name>."); return 1; }
        std::string id = remove_parser.get<std::string>("id");
        std::string repoRoot = exeDir + "/repos/" + selectedRepoName;
        core::RepoManager repo(repoRoot);
        if (!repo.loadIndex()) { logger::error("Failed to load repository index"); return 1; }
        if (!repo.removeItem(id)) { logger::error("Failed to remove item: " + id); return 1; }
        return 0;
    } else if (program.is_subcommand_used("rename")) {
        std::string repoFlag = program.get<std::string>("--repo");
        std::string selectedRepoName = getSelectedRepoName(repoFlag);
        if (selectedRepoName.empty()) { logger::error("No repo selected. Use 'use <name>' or add --repo <name>."); return 1; }
        std::string id = rename_parser.get<std::string>("id");
        std::string newName = rename_parser.get<std::string>("new_name");
        std::string repoRoot = exeDir + "/repos/" + selectedRepoName;
        core::RepoManager repo(repoRoot);
        if (!repo.loadIndex()) { logger::error("Failed to load repository index"); return 1; }
        if (!repo.renameItem(id, newName)) { logger::error("Failed to rename item: " + id); return 1; }
        return 0;
    } else if (program.is_subcommand_used("repl")) {
        repl();
        return 0;
    } else if (program.is_subcommand_used("list-repos")) {
        std::string reposDir = exeDir + "/repos";
        if (!std::filesystem::exists(reposDir)) {
            logger::info("No repositories found");
            return 0;
        }
        
        std::cout << "Local repositories:\n";
        for (const auto& entry : std::filesystem::directory_iterator(reposDir)) {
            if (entry.is_directory()) {
                std::string repoName = entry.path().filename().string();
                std::string indexPath = entry.path().string() + "/index.json";
                
                if (std::filesystem::exists(indexPath)) {
                    // Try to read repo info from index.json
                    try {
                        std::ifstream file(indexPath);
                        nlohmann::json j;
                        file >> j;
                        std::string desc = j.value("repositoryDescription", "");
                        std::string name = j.value("repositoryName", repoName);
                        std::cout << "  " << repoName;
                        if (name != repoName) std::cout << " (" << name << ")";
                        if (!desc.empty()) std::cout << " - " << desc;
                        std::cout << "\n";
                    } catch (...) {
                        std::cout << "  " << repoName << " (invalid index.json)\n";
                    }
                } else {
                    std::cout << "  " << repoName << " (no index.json)\n";
                }
            }
        }
        return 0;
    } else if (program.is_subcommand_used("delete-repo")) {
        std::string name = delete_repo_parser.get<std::string>("name");
        bool force = delete_repo_parser.get<bool>("--force");
        std::string repoRoot = exeDir + "/repos/" + name;
        
        if (!std::filesystem::exists(repoRoot)) {
            logger::error("Repository not found: " + name);
            return 1;
        }
        
        // Confirmation prompt unless --force is used
        if (!force) {
            std::cout << "Are you sure you want to delete repository '" << name << "'? This action cannot be undone. (y/N): ";
            std::string response;
            std::getline(std::cin, response);
            if (response != "y" && response != "Y" && response != "yes") {
                logger::info("Deletion cancelled");
                return 0;
            }
        }
        
        // Check if this is the current repo and clear it
        // config already loaded
        if (config::getCurrentRepo() == name) {
            config::setCurrentRepo("");
            config::saveConfig(getConfigPath());
            logger::info("Cleared current repository selection");
        }
        
        // Remove the entire directory
        try {
            // Use a platform-native removal to avoid locked/attribute issues
#ifdef _WIN32
            std::string cmd = std::string("cmd /C rmdir /S /Q \"") + repoRoot + "\"";
            int rc = std::system(cmd.c_str());
            if (rc != 0 && std::filesystem::exists(repoRoot)) {
                throw std::runtime_error("rmdir failed with code " + std::to_string(rc));
            }
#else
            std::error_code ec;
            std::filesystem::remove_all(repoRoot, ec);
            if (ec && std::filesystem::exists(repoRoot)) {
                throw std::runtime_error("remove_all failed: " + ec.message());
            }
#endif
            logger::info("Deleted repository: " + name);
        } catch (const std::exception& e) {
            logger::error("Failed to delete repository: " + std::string(e.what()));
            logger::error("Try manually deleting: " + repoRoot);
            return 1;
        }
        return 0;
    } else if (program.is_subcommand_used("rename-repo")) {
        std::string oldName = rename_repo_parser.get<std::string>("old_name");
        std::string newName = rename_repo_parser.get<std::string>("new_name");
        std::string oldRepoRoot = exeDir + "/repos/" + oldName;
        std::string newRepoRoot = exeDir + "/repos/" + newName;
        
        if (!std::filesystem::exists(oldRepoRoot)) {
            logger::error("Repository not found: " + oldName);
            return 1;
        }
        
        if (std::filesystem::exists(newRepoRoot)) {
            logger::error("Repository already exists: " + newName);
            return 1;
        }
        
        // Rename the directory
        try {
            std::filesystem::rename(oldRepoRoot, newRepoRoot);
            logger::info("Renamed repository from '" + oldName + "' to '" + newName + "'");
            
            // Update current repo if it was the renamed one
            // config already loaded
            if (config::getCurrentRepo() == oldName) {
                config::setCurrentRepo(newName);
                config::saveConfig(getConfigPath());
                logger::info("Updated current repository selection");
            }
        } catch (const std::exception& e) {
            logger::error("Failed to rename repository: " + std::string(e.what()));
            return 1;
        }
        return 0;
    } else if (program.is_subcommand_used("gh-login")) {
        // Provide quick links for creating a GitHub token
        std::cout << "Create a GitHub Personal Access Token (classic) with 'repo' and 'delete_repo' scopes:\n"
                  << "  https://github.com/settings/tokens/new?scopes=repo,delete_repo&description=RepoMan%20CLI\n"
                  << "Or create a fine-grained token and grant access to desired repos (Administration: Read and write; Contents: Read and write):\n"
                  << "  https://github.com/settings/personal-access-tokens/new\n\n";

        // Ask for token securely (no echo). Minimal cross-platform approach:
        std::string token;
        std::cout << "Enter GitHub token (input hidden): ";
#ifdef _WIN32
        {
            HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
            DWORD mode = 0; GetConsoleMode(hStdin, &mode);
            DWORD oldMode = mode;
            mode &= ~ENABLE_ECHO_INPUT;
            SetConsoleMode(hStdin, mode);
            std::getline(std::cin, token);
            SetConsoleMode(hStdin, oldMode);
            std::cout << "\n";
        }
#else
        {
            int rc_stty1 = std::system("stty -echo 2>/dev/null"); (void)rc_stty1;
            std::getline(std::cin, token);
            int rc_stty2 = std::system("stty echo 2>/dev/null"); (void)rc_stty2;
            std::cout << "\n";
        }
#endif
        if (token.empty()) { logger::error("Empty token"); return 1; }
        // Validate token by calling GitHub API (basic): GET user
        // We use curl via system to avoid adding heavy deps here
        std::string tmpFile = exeDir + "/gh_user.json";
        std::string cmd = std::string("curl -s -H \"Authorization: token ") + token + "\" https://api.github.com/user -o \"" + tmpFile + "\"";
        int rc = std::system(cmd.c_str());
        if (rc != 0) { logger::error("curl failed"); return 1; }
        // Parse login
        std::ifstream fin(tmpFile);
        nlohmann::json j; fin >> j; fin.close();
        std::filesystem::remove(tmpFile);
        if (!j.contains("login")) { logger::error("Invalid token"); return 1; }
        std::string login = j["login"].get<std::string>();
        // Encrypt token (simple XOR with exe path hash as key). For Windows we could use DPAPI later.
        auto keyStr = exeDir;
        uint8_t key = 0x5A;
        for (char c : keyStr) key ^= static_cast<uint8_t>(c);
        std::string enc = token;
        for (auto& ch : enc) ch = static_cast<char>(static_cast<uint8_t>(ch) ^ key);
        // Store in config (config already loaded)
        config::Config::getInstance().setGithubUser(login);
        config::Config::getInstance().setGithubTokenEncrypted(enc);
        if (!config::saveConfig(getConfigPath())) { logger::error("Failed to save token"); return 1; }
        logger::info("GitHub login saved for user: " + login);
        return 0;
    } else if (program.is_subcommand_used("gh-list")) {
        // List user repos using stored token (config already loaded)
        std::string enc = config::Config::getInstance().getGithubTokenEncrypted();
        if (enc.empty()) { logger::error("No token saved. Run gh-login first."); return 1; }
        auto keyStr = exeDir; uint8_t key = 0x5A; for (char c : keyStr) key ^= static_cast<uint8_t>(c);
        std::string token = enc; for (auto& ch : token) ch = static_cast<char>(static_cast<uint8_t>(ch) ^ key);
        std::string tmpFile = exeDir + "/gh_repos.json";
        std::string cmd = std::string("curl -s -H \"Authorization: token ") + token + "\" https://api.github.com/user/repos?per_page=100 -o \"" + tmpFile + "\"";
        int rc = std::system(cmd.c_str());
        if (rc != 0) { logger::error("curl failed"); return 1; }
        std::ifstream fin(tmpFile);
        nlohmann::json arr; fin >> arr; fin.close(); std::filesystem::remove(tmpFile);
        if (!arr.is_array()) { logger::error("Unexpected GitHub response"); return 1; }
        for (const auto& r : arr) {
            std::string full = r.contains("full_name") && r["full_name"].is_string() ? r["full_name"].get<std::string>() : "?";
            std::string priv = r.contains("private") && r["private"].is_boolean() && r["private"].get<bool>() ? "private" : "public";
            std::string desc = r.contains("description") && r["description"].is_string() ? r["description"].get<std::string>() : "";
            std::string branch = r.contains("default_branch") && r["default_branch"].is_string() ? r["default_branch"].get<std::string>() : std::string("main");

            // Check compatibility by trying to fetch index.json from default branch
            std::string tmpIdx = exeDir + "/gh_idx.json";
            std::string rawUrl = std::string("https://raw.githubusercontent.com/") + full + "/" + branch + "/index.json";
            std::string getIdx = std::string("curl -sL -H \"Authorization: token ") + token + "\" -H \"Accept: application/vnd.github.v3.raw\" \"" + rawUrl + "\" -o \"" + tmpIdx + "\"";
            int rc2 = std::system(getIdx.c_str()); (void)rc2;
            bool compatible = false;
            try {
                std::ifstream fi(tmpIdx);
                if (fi.good()) {
                    nlohmann::json jidx; fi >> jidx; fi.close();
                    compatible = jidx.is_object() && jidx.contains("version") && jidx.contains("items") && jidx["items"].is_array();
                }
            } catch (...) { /* ignore */ }
            std::filesystem::remove(tmpIdx);

            std::cout << full << "  [" << priv << "]";
            if (compatible) std::cout << "  [compatible]";
            if (!desc.empty()) std::cout << "  " << desc;
            std::cout << "\n";
        }
        return 0;
    } else if (program.is_subcommand_used("gh-token-check")) {
        // Show token scopes and missing capabilities for delete/visibility
        // config already loaded
        std::string encTok = config::Config::getInstance().getGithubTokenEncrypted();
        if (encTok.empty()) { logger::error("No token saved. Run gh-login first."); return 1; }
        auto keyStr2 = exeDir; uint8_t key2 = 0x5A; for (char c : keyStr2) key2 ^= static_cast<uint8_t>(c);
        std::string token2 = encTok; for (auto& ch : token2) ch = static_cast<char>(static_cast<uint8_t>(ch) ^ key2);

        // Fetch headers to read X-OAuth-Scopes (only for classic tokens)
        std::string headCmd = std::string("curl -s -I -H \"Authorization: token ") + token2 + "\" https://api.github.com/user";
        int rc_hdr = std::system((headCmd + " -o /dev/null").c_str()); (void)rc_hdr;
        std::string hdrs = exeDir + "/gh_scopes.h";
        std::string headCmd2 = headCmd + " -D \"" + hdrs + "\" -o /dev/null";
        int rc_hdr2 = std::system(headCmd2.c_str()); (void)rc_hdr2;
        std::ifstream hf(hdrs);
        std::string line; std::string scopes;
        while (std::getline(hf, line)) {
            auto pos = line.find("X-OAuth-Scopes:");
            if (pos != std::string::npos) { scopes = line.substr(pos + 15); break; }
        }
        hf.close(); std::filesystem::remove(hdrs);

        // Fetch user to show token owner
        std::string userFile = exeDir + "/gh_user_check.json";
        std::string getUser = std::string("curl -s -H \"Authorization: token ") + token2 + "\" https://api.github.com/user -o \"" + userFile + "\"";
        int rc_user = std::system(getUser.c_str()); (void)rc_user;
        std::string owner;
        try {
            std::ifstream fin(userFile);
            if (fin.good()) {
                nlohmann::json uj; fin >> uj; fin.close();
                if (uj.contains("login") && uj["login"].is_string()) owner = uj["login"].get<std::string>();
            }
        } catch (...) {}
        std::filesystem::remove(userFile);
        if (!owner.empty()) std::cout << "Token owner: " << owner << "\n";

        std::string trimmed = scopes;
        // trim spaces and commas
        while (!trimmed.empty() && (trimmed.front()==' '||trimmed.front()==',')) trimmed.erase(trimmed.begin());
        while (!trimmed.empty() && (trimmed.back()==' '||trimmed.back()==','||trimmed.back()=='\r'||trimmed.back()=='\n')) trimmed.pop_back();

        if (!trimmed.empty()) {
            std::cout << "Token scopes: " << trimmed << "\n";
            bool hasRepo = trimmed.find("repo") != std::string::npos;
            bool hasDelete = trimmed.find("delete_repo") != std::string::npos;
            if (!hasRepo) std::cout << "- Missing 'repo' (classic) or appropriate fine-grained permissions\n";
            if (!hasDelete) std::cout << "- Missing 'delete_repo' for GitHub repo deletion (classic)\n";
        } else {
            std::cout << "Token is likely fine-grained or scopes header not exposed by GitHub for this token.\n";
            std::cout << "Scopes cannot be inferred via API; ensure repository permissions: Administration (Read and write) and Contents (Read and write).\n";
        }
        return 0;
    } else if (program.is_subcommand_used("gh-pull")) {
        // Pull latest from GitHub into current local repo; ensure compatibility like gh-list
        std::string remote = gh_pull_parser.get<std::string>("--remote");
        std::string branch = gh_pull_parser.get<std::string>("--branch");
        // Derive default remote from current repo name (user-owned) if not provided
        if (remote.empty()) {
            std::string selectedRepoName = getSelectedRepoName("");
            if (selectedRepoName.empty()) { logger::error("No repo selected. Use 'use <name>'."); return 1; }
            // config already loaded
            std::string loginUser = config::Config::getInstance().getGithubUser();
            remote = loginUser.empty() ? selectedRepoName : (loginUser + "/" + selectedRepoName);
        }

        // Token (config already loaded)
        std::string enc = config::Config::getInstance().getGithubTokenEncrypted();
        if (enc.empty()) { logger::error("No token saved. Run gh-login first."); return 1; }
        auto keyStr = exeDir; uint8_t key = 0x5A; for (char c : keyStr) key ^= static_cast<uint8_t>(c);
        std::string token = enc; for (auto& ch : token) ch = static_cast<char>(static_cast<uint8_t>(ch) ^ key);

        // Current local repo root
        std::string selectedRepoName = getSelectedRepoName("");
        if (selectedRepoName.empty()) { logger::error("No repo selected. Use 'use <name>'."); return 1; }
        std::string repoRoot = exeDir + "/repos/" + selectedRepoName;
        if (!std::filesystem::exists(repoRoot)) { logger::error("Local repo not found"); return 1; }

        // Compatibility check: fetch index.json from remote branch
        std::string tmpIdx = exeDir + "/gh_idx_pull.json";
        std::string rawUrl = std::string("https://raw.githubusercontent.com/") + remote + "/" + branch + "/index.json";
        std::string getIdx = std::string("curl -sL -H \"Authorization: token ") + token + "\" -H \"Accept: application/vnd.github.v3.raw\" \"" + rawUrl + "\" -o \"" + tmpIdx + "\"";
        int rcIdx = std::system(getIdx.c_str()); (void)rcIdx;
        bool compatible = false;
        try {
            std::ifstream fi(tmpIdx);
            if (fi.good()) {
                nlohmann::json jidx; fi >> jidx; fi.close();
                compatible = jidx.is_object() && jidx.contains("version") && jidx.contains("items") && jidx["items"].is_array();
            }
        } catch (...) {}
        std::filesystem::remove(tmpIdx);
        if (!compatible) { logger::error("Remote repo is not compatible (missing index.json or invalid format)"); return 1; }

        // Download zipball of branch
        std::string zip = exeDir + "/pull_tmp.zip";
        std::string url = "https://api.github.com/repos/" + remote + "/zipball/" + branch;
        std::string cmd = std::string("curl -L -H \"Authorization: token ") + token + "\" -o \"" + zip + "\" \"" + url + "\"";
        int rc = std::system(cmd.c_str()); if (rc != 0) { logger::error("download failed"); return 1; }

        // Unzip into temp and overlay contents onto current repo
        std::string unzipDir = exeDir + "/pull_unzip_tmp";
        fs::createDirectoryIfNotExists(unzipDir);
        {
            std::string unzipErr;
            if (!ziputil::extractArchive(zip, unzipDir, unzipErr)) {
                logger::error("unzip failed: " + unzipErr);
                return 1;
            }
        }

        // Find top-level folder and copy its contents into repoRoot (overwrite)
        std::string top;
        for (auto& e : std::filesystem::directory_iterator(unzipDir)) { if (e.is_directory()) { top = e.path().string(); break; } }
        if (top.empty()) { logger::error("unexpected zip layout"); return 1; }

        // Copy recursively (overwrite). Be robust on Windows/MinGW where overwrite flags can be flaky.
        for (auto& p : std::filesystem::recursive_directory_iterator(top)) {
            if (!p.is_regular_file()) continue;
            auto rel = std::filesystem::relative(p.path(), top).string();
            std::filesystem::path dest = std::filesystem::path(repoRoot) / rel;
            std::filesystem::create_directories(dest.parent_path());
            try {
                if (std::filesystem::exists(dest)) {
                    std::error_code ec_rm;
                    std::filesystem::remove(dest, ec_rm);
                }
                std::error_code ec_cp;
                std::filesystem::copy_file(p.path(), dest, std::filesystem::copy_options::overwrite_existing, ec_cp);
                if (ec_cp) {
                    throw std::runtime_error(std::string("copy_file failed: ") + ec_cp.message());
                }
            } catch (const std::exception& e) {
                logger::error(std::string("copy failed: ") + e.what());
                std::filesystem::remove(zip);
                std::filesystem::remove_all(unzipDir);
                return 1;
            }
        }

        // Cleanup
        std::filesystem::remove(zip);
        std::filesystem::remove_all(unzipDir);
        logger::info("Pulled and updated repo from github.com/" + remote + " (branch " + branch + ")");
        return 0;
    } else if (program.is_subcommand_used("gh-clone")) {
        // Clone compatible repo into local repos/<name>
        std::string remote = gh_clone_parser.get<std::string>("remote");
        std::string branch = gh_clone_parser.get<std::string>("--branch");
        std::string name = gh_clone_parser.get<std::string>("--name");
        bool setCur = false;
        try { setCur = gh_clone_parser.get<bool>("--set-current"); } catch (...) { setCur = false; }

        // config already loaded
        std::string enc = config::Config::getInstance().getGithubTokenEncrypted();
        if (enc.empty()) { logger::error("No token saved. Run gh-login first."); return 1; }
        auto keyStr = exeDir; uint8_t key = 0x5A; for (char c : keyStr) key ^= static_cast<uint8_t>(c);
        std::string token = enc; for (auto& ch : token) ch = static_cast<char>(static_cast<uint8_t>(ch) ^ key);

        if (name.empty()) {
            // default local name = repo part
            auto slash = remote.find('/');
            name = (slash != std::string::npos) ? remote.substr(slash+1) : remote;
        }
        std::string repoRoot = exeDir + "/repos/" + name;
        if (std::filesystem::exists(repoRoot)) { logger::error("Local repo already exists: " + name); return 1; }
        fs::createDirectoryIfNotExists(exeDir + "/repos");

        // Download zipball of branch
        std::string zip = exeDir + "/" + name + ".zip";
        std::string url = "https://api.github.com/repos/" + remote + "/zipball/" + branch;
        std::string cmd = std::string("curl -L -H \"Authorization: token ") + token + "\" -o \"" + zip + "\" \"" + url + "\"";
        int rc = std::system(cmd.c_str()); if (rc != 0) { logger::error("download failed"); return 1; }

        // Unzip
        std::string unzipDir = exeDir + "/ziptmp_" + name;
        fs::createDirectoryIfNotExists(unzipDir);
        {
            std::string unzipErr;
            if (!ziputil::extractArchive(zip, unzipDir, unzipErr)) { logger::error("unzip failed: " + unzipErr); return 1; }
        }

        // Move extracted single top-level folder to repos/<name>
        std::string top;
        for (auto& e : std::filesystem::directory_iterator(unzipDir)) { if (e.is_directory()) { top = e.path().string(); break; } }
        if (top.empty()) { logger::error("unexpected zip layout"); return 1; }
        std::filesystem::rename(top, repoRoot);
        std::filesystem::remove_all(unzipDir);
        std::filesystem::remove(zip);

        // Validate index.json presence
        if (!std::filesystem::exists(repoRoot + "/index.json")) {
            logger::warning("index.json not found in remote repo; may be incompatible");
        }

        if (setCur) {
            config::setCurrentRepo(name);
            if (!config::saveConfig(getConfigPath())) { logger::error("Failed to save config"); return 1; }
            logger::info("Selected repo: " + name);
        }
        logger::info("Cloned to repos/" + name);
        return 0;
    } else if (program.is_subcommand_used("gh-push")) {
        // Push local repo contents to GitHub
        std::string remote;
        std::string branch;
        bool createRepo = false;
        bool makePrivate = false;
        bool forcePush = false;
        try { remote = gh_push_parser.get<std::string>("--remote"); } catch (...) { remote = ""; }
        try { branch = gh_push_parser.get<std::string>("--branch"); } catch (...) { branch = "main"; }
        try { createRepo = gh_push_parser.get<bool>("--create"); } catch (...) {}
        try { makePrivate = gh_push_parser.get<bool>("--private"); } catch (...) {}
        try { forcePush = gh_push_parser.get<bool>("--force"); } catch (...) {}
        
        logger::debug("gh-push parameters:");
        logger::debug("  remote: '" + remote + "'");
        logger::debug("  branch: '" + branch + "'");
        logger::debug("  createRepo: " + std::string(createRepo ? "true" : "false"));
        logger::debug("  makePrivate: " + std::string(makePrivate ? "true" : "false"));
        logger::debug("  forcePush: " + std::string(forcePush ? "true" : "false"));

        config::loadConfig(getConfigPath());
        std::string enc = config::Config::getInstance().getGithubTokenEncrypted();
        if (enc.empty()) { logger::error("No token saved. Run gh-login first."); return 1; }
        auto keyStr = exeDir; uint8_t key = 0x5A; for (char c : keyStr) key ^= static_cast<uint8_t>(c);
        std::string token = enc; for (auto& ch : token) ch = static_cast<char>(static_cast<uint8_t>(ch) ^ key);

        // Pick current repo
        std::string selectedRepoName = getSelectedRepoName("");
        if (selectedRepoName.empty()) { logger::error("No repo selected. Use 'use <name>'."); return 1; }
        std::string repoRoot = exeDir + "/repos/" + selectedRepoName;
        if (!std::filesystem::exists(repoRoot)) { logger::error("Local repo not found"); return 1; }

        // Derive default remote from current repo name if not provided
        if (remote.empty()) {
            remote = selectedRepoName; // user-owned, will require --create if missing
        }

        // Build owner/repo path. If no owner provided, use saved GitHub login (config already loaded)
        std::string loginUser = config::Config::getInstance().getGithubUser();
        bool hasOwner = (remote.find('/') != std::string::npos);
        std::string remotePath = hasOwner ? remote : (loginUser.empty() ? remote : (loginUser + "/" + remote));

        // Optionally create repository (for user-owned remotes)
        logger::debug("Checking if repository creation is needed...");
        logger::debug("  createRepo flag: " + std::string(createRepo ? "true" : "false"));
        logger::debug("  remotePath: '" + remotePath + "'");
        if (createRepo) {
            logger::debug("Starting repository creation process...");
            auto slash = remote.find('/');
            std::string tmp = exeDir + "/gh_create.json";
            std::string body;
            std::string url;
            if (slash != std::string::npos) {
                // Create under organization
                std::string owner = remote.substr(0, slash);
                std::string repoOnly = remote.substr(slash + 1);
                body = std::string("{\"name\":\"") + repoOnly + "\",\"private\":" + (makePrivate?"true":"false") + "}";
                url = std::string("https://api.github.com/orgs/") + owner + "/repos";
            } else {
                // Create under user
                body = std::string("{\"name\":\"") + remote + "\",\"private\":" + (makePrivate?"true":"false") + "}";
                url = "https://api.github.com/user/repos";
            }
            // Write request body to a temp file to avoid shell quoting issues on Windows
            std::string bodyFile = exeDir + "/gh_create_body.json";
            try {
                std::ofstream bf(bodyFile, std::ios::binary);
                bf << body;
                bf.close();
            } catch (...) {
                logger::error("Failed to write request body file");
                return 1;
            }
            std::string cmd = std::string("curl -s -X POST -H \"Authorization: token ") + token + "\" -H \"Accept: application/vnd.github+json\" -H \"Content-Type: application/json\" --data @" + bodyFile + " \"" + url + "\" -o \"" + tmp + "\"";
            logger::debug("Creating repository with API call: " + url);
            logger::debug("Request body: " + body);
            int rcC = std::system(cmd.c_str()); 
            logger::debug("curl exit code: " + std::to_string(rcC));
            bool createdOk = false; std::string errMsg; std::string createdFullName;
            try {
                std::ifstream jf(tmp);
                if (jf.good()) {
                    nlohmann::json jr; jf >> jr; jf.close();
                    logger::debug("GitHub API response: " + jr.dump());
                    if (jr.contains("full_name") && jr["full_name"].is_string()) {
                        createdOk = true;
                        createdFullName = jr["full_name"].get<std::string>();
                        logger::debug("Repository created successfully: " + createdFullName);
                    } else if (jr.contains("message") && jr["message"].is_string()) {
                        errMsg = jr["message"].get<std::string>();
                        logger::debug("GitHub API error message: " + errMsg);
                    } else if (jr.contains("errors") && jr["errors"].is_array()) {
                        std::string errors = jr["errors"].dump();
                        errMsg = "API errors: " + errors;
                        logger::debug("GitHub API validation errors: " + errors);
                    }
                } else {
                    logger::debug("Failed to read response file: " + tmp);
                }
            } catch (const std::exception& e) {
                logger::debug("Exception parsing GitHub response: " + std::string(e.what()));
            }
            std::filesystem::remove(tmp);
            std::filesystem::remove(bodyFile);
            if (!createdOk) {
                if (errMsg.empty()) errMsg = "failed to create repo - no response or unknown error";
                logger::error("GitHub create repo failed: " + errMsg);
                logger::debug("remotePath was: " + remotePath);
                return 1;
            }
            if (!createdFullName.empty()) {
                remotePath = createdFullName;
            }
        }

        // Prepare git repo: ensure branch checked out, remote set, integrate upstream, then commit and push
        // Set local identity to avoid leaking host username/hostname
        {
            std::string ghUser = config::Config::getInstance().getGithubUser();
            if (ghUser.empty()) ghUser = "github-user";
            // Keep header local to this TU to avoid extra deps
            const char* appName = "RepoMan";
            const char* appVersion = "0.0.2";
            std::string appSig = std::string(appName) + "/" + appVersion;
            std::string cfgName = std::string("cd \"") + repoRoot + "\" && git config user.name \"" + ghUser + " (" + appSig + ")\"";
            std::string cfgEmail = std::string("cd \"") + repoRoot + "\" && git config user.email \"" + ghUser + "@users.noreply.github.com\"";
            std::system(cfgName.c_str());
            std::system(cfgEmail.c_str());
        }
        std::string https = std::string("https://") + token + "@github.com/" + remotePath + ".git";
#ifdef _WIN32
        std::string cmdInitOnly = std::string("cd \"") + repoRoot + "\" && git init";
        std::string cmdSetUrl = std::string("cd \"") + repoRoot + "\" && git remote set-url origin \"" + https + "\"";
        std::string cmdAddOrigin = std::string("cd \"") + repoRoot + "\" && git remote add origin \"" + https + "\"";
        std::string cmdCheckout = std::string("cd \"") + repoRoot + "\" && git checkout -B " + branch;
        std::string cmdCommit = std::string("cd \"") + repoRoot + "\" && git add . && (git diff --cached --quiet || git commit -m \"Update via RepoMan\")";
        std::string cmdPullRb = std::string("cd \"") + repoRoot + "\" && git pull --rebase origin " + branch + " 2>NUL 1>NUL";
        std::string cmdPush1 = std::string("cd \"") + repoRoot + "\" && git push -u origin " + branch;
        std::string cmdPushF = std::string("cd \"") + repoRoot + "\" && git push -f -u origin " + branch;
#else
        std::string cmdInitOnly = std::string("cd \"") + repoRoot + "\" && git init";
        std::string cmdSetUrl = std::string("cd \"") + repoRoot + "\" && git remote set-url origin \"" + https + "\"";
        std::string cmdAddOrigin = std::string("cd \"") + repoRoot + "\" && git remote add origin \"" + https + "\"";
        std::string cmdCheckout = std::string("cd \"") + repoRoot + "\" && git checkout -B " + branch;
        std::string cmdCommit = std::string("cd \"") + repoRoot + "\" && git add . && (git diff --cached --quiet || git commit -m \"Update via RepoMan\")";
        std::string cmdPullRb = std::string("cd \"") + repoRoot + "\" && git pull --rebase origin " + branch + " 2>/dev/null 1>/dev/null";
        std::string cmdPush1 = std::string("cd \"") + repoRoot + "\" && git push -u origin " + branch;
        std::string cmdPushF = std::string("cd \"") + repoRoot + "\" && git push -f -u origin " + branch;
#endif
        int rc_init = std::system(cmdInitOnly.c_str()); (void)rc_init;
        int rc_seturl = std::system(cmdSetUrl.c_str()); (void)rc_seturl;
        int rc_addorigin = std::system(cmdAddOrigin.c_str()); (void)rc_addorigin;
        int rc_checkout = std::system(cmdCheckout.c_str()); (void)rc_checkout;
        int rc_commit = std::system(cmdCommit.c_str()); (void)rc_commit;
        int rc_pullrb = std::system(cmdPullRb.c_str()); (void)rc_pullrb;
        int rc = 0;
        if (forcePush) {
            logger::debug("gh-push: force flag set, pushing with -f");
            rc = std::system(cmdPushF.c_str());
        } else {
            rc = std::system(cmdPush1.c_str());
            if (rc != 0) {
                logger::debug("gh-push: initial push failed, pulling with --rebase and retrying");
                int rc_pullrb2 = std::system(cmdPullRb.c_str()); (void)rc_pullrb2;
                rc = std::system(cmdPush1.c_str());
                if (rc != 0) {
                    logger::debug("gh-push: retry push failed, forcing final push with -f");
                    rc = std::system(cmdPushF.c_str());
                }
            }
        }
        if (rc != 0) { logger::error("git push failed"); return 1; }
        logger::info("Pushed to github.com/" + remotePath + " (branch " + branch + ")");
        return 0;
    } else if (program.is_subcommand_used("gh-delete")) {
        std::string remote = gh_delete_parser.get<std::string>("remote");
        bool force = gh_delete_parser.get<bool>("--force");
        if (!force) {
            std::cout << "Delete repo '" << remote << "' on GitHub? (y/N): ";
            std::string resp; std::getline(std::cin, resp);
            if (resp != "y" && resp != "Y" && resp != "yes") { logger::info("Cancelled"); return 0; }
        }
        // config already loaded
        std::string enc = config::Config::getInstance().getGithubTokenEncrypted();
        if (enc.empty()) { logger::error("No token saved. Run gh-login first."); return 1; }
        auto keyStr = exeDir; uint8_t key = 0x5A; for (char c : keyStr) key ^= static_cast<uint8_t>(c);
        std::string token = enc; for (auto& ch : token) ch = static_cast<char>(static_cast<uint8_t>(ch) ^ key);

        std::string tmp = exeDir + "/gh_delete.json";
        std::string codeFile = exeDir + "/gh_delete.code";
        std::string cmd = std::string("curl -s -X DELETE -H \"Authorization: token ") + token + "\" \"https://api.github.com/repos/" + remote + "\" -o \"" + tmp + "\" -w \"%{http_code}\" > \"" + codeFile + "\"";
        int rc = std::system(cmd.c_str());
        if (rc != 0) { logger::error("delete failed"); return 1; }
        int httpCode = -1;
        try {
            std::ifstream cf(codeFile);
            if (cf.good()) { cf >> httpCode; }
        } catch (...) {}
        std::filesystem::remove(codeFile);
        if (httpCode != 204) {
            std::string errMsg;
            try {
                std::ifstream jf(tmp);
                if (jf.good()) {
                    nlohmann::json jr; jf >> jr; jf.close();
                    if (jr.contains("message") && jr["message"].is_string()) errMsg = jr["message"].get<std::string>();
                }
            } catch (...) {}
            std::filesystem::remove(tmp);
            if (httpCode == 404) {
                logger::warning("Repository not found on GitHub: " + remote);
                return 0;
            }
            if (errMsg.empty()) errMsg = "unexpected HTTP " + std::to_string(httpCode);
            logger::error("GitHub delete failed: " + errMsg);
            return 1;
        }
        std::filesystem::remove(tmp);
        logger::info("Deleted github.com/" + remote);
        return 0;
    } else if (program.is_subcommand_used("gh-visibility")) {
        std::string remote = gh_visibility_parser.get<std::string>("remote");
        std::string vis = gh_visibility_parser.get<std::string>("visibility");
        bool makePrivate = (vis == "private");

        // config already loaded
        std::string enc = config::Config::getInstance().getGithubTokenEncrypted();
        if (enc.empty()) { logger::error("No token saved. Run gh-login first."); return 1; }
        auto keyStr = exeDir; uint8_t key = 0x5A; for (char c : keyStr) key ^= static_cast<uint8_t>(c);
        std::string token = enc; for (auto& ch : token) ch = static_cast<char>(static_cast<uint8_t>(ch) ^ key);

        std::string tmp = exeDir + "/gh_update.json";
        std::string body = std::string("{\"private\":") + (makePrivate?"true":"false") + "}";
        // Write body to file to avoid Windows quoting issues
        std::string bodyFile = exeDir + "/gh_update_body.json";
        try {
            std::ofstream bf(bodyFile, std::ios::binary);
            bf << body;
            bf.close();
        } catch (...) {
            logger::error("Failed to write request body file");
            return 1;
        }
        std::string cmd = std::string("curl -s -X PATCH -H \"Authorization: token ") + token + "\" -H \"Accept: application/vnd.github+json\" -H \"Content-Type: application/json\" --data @" + bodyFile + " https://api.github.com/repos/" + remote + " -o \"" + tmp + "\"";
        logger::debug("Updating repo visibility via API: github.com/" + remote + " -> " + vis);
        int rc = std::system(cmd.c_str()); (void)rc;
        bool ok = false; bool finalPrivate = makePrivate; std::string errMsg;
        try {
            std::ifstream jf(tmp);
            if (jf.good()) {
                nlohmann::json jr; jf >> jr; jf.close();
                logger::debug("GitHub API response: " + jr.dump());
                if (jr.contains("private") && jr["private"].is_boolean()) {
                    ok = true; finalPrivate = jr["private"].get<bool>();
                } else if (jr.contains("message") && jr["message"].is_string()) {
                    errMsg = jr["message"].get<std::string>();
                    if (jr.contains("errors")) errMsg += std::string(" ") + jr["errors"].dump();
                }
            }
        } catch (const std::exception& e) {
            logger::debug(std::string("Exception parsing GitHub response: ") + e.what());
        }
        std::filesystem::remove(tmp);
        std::filesystem::remove(bodyFile);
        if (!ok) {
            if (errMsg.empty()) errMsg = "failed to update visibility";
            logger::error("GitHub visibility update failed: " + errMsg);
            logger::error("Ensure the token has sufficient permissions and you have admin access to the repo.");
            return 1;
        }
        if (finalPrivate != makePrivate) {
            logger::error("Visibility unchanged by GitHub (permission/policy may prevent change)");
            return 1;
        }
        logger::info("Updated visibility for github.com/" + remote + " to " + vis);
        return 0;
    }
    
    // If no subcommand is used, print help
    std::cerr << program;
    return 1;
}

void repl() {
    logger::info("Entering interactive mode. Type 'help' or 'exit'.");
    std::string line;
    static std::vector<std::string> history;
#ifndef _WIN32
    // Persistent history under home
    std::string histPath;
    if (const char* home = std::getenv("HOME")) histPath = std::string(home) + "/.repoman_history";
    if (!histPath.empty()) utils::loadHistory(histPath, history);
#endif

    auto completer = [&](const std::string& buffer, size_t cursor) -> std::vector<std::string> {
        // Current token
        size_t start = cursor; while (start > 0 && !isspace(static_cast<unsigned char>(buffer[start-1]))) --start;
        std::string token = buffer.substr(start, cursor - start);
        std::vector<std::string> cmds = {
            "help","exit","quit","init","use","add","list","index","remove","rename",
            "list-repos","delete-repo","rename-repo","gh-login","gh-list","gh-clone","gh-pull",
            "gh-push","gh-delete","gh-visibility","verify","gh-token-check"
        };
        std::vector<std::string> out;
        if (start == 0) {
            for (const auto& c : cmds) if (c.rfind(token, 0) == 0) out.push_back(c);
        } else {
            // after command: suggest repo names for use/add/index/remove/rename/gh-*
            std::string reposDir = fs::getExecutablePath() + "/repos";
            if (std::filesystem::exists(reposDir)) {
                for (const auto& e : std::filesystem::directory_iterator(reposDir)) if (e.is_directory()) {
                    std::string name = e.path().filename().string();
                    if (name.rfind(token, 0) == 0) out.push_back(name);
                }
            }
        }
        return out;
    };
    while (true) {
        if (!utils::readLineInteractive("> ", line, history, completer)) break;
        if (line == "exit" || line == "quit") break;
        if (line == "help") {
            // Re-create a dummy program to print help for all subcommands
            argparse::ArgumentParser program("repoman-cli");
            program.add_description("RepoMan - manage Quake 3 content repositories");
            program.add_epilog(
                "IMPORTANT / ВАЖНО: ASCII-only file and path names are required.\n"
                "Unicode (e.g., Cyrillic) in file names or directories is not supported\n"
                "and may result in corrupted paths or indexing failures.\n"
                "Только ASCII-имена файлов и путей поддерживаются. Юникод (например, кириллица)\n"
                "в именах файлов/папок не поддерживается и приведёт к ошибкам.");
            argparse::ArgumentParser init_parser("init");
            argparse::ArgumentParser use_parser("use");
            argparse::ArgumentParser add_parser("add");
            argparse::ArgumentParser list_parser("list");
            argparse::ArgumentParser index_parser("index");
            argparse::ArgumentParser remove_parser("remove");
            argparse::ArgumentParser rename_parser("rename");
            argparse::ArgumentParser repl_parser("repl");
            argparse::ArgumentParser list_repos_parser("list-repos");
            argparse::ArgumentParser gh_login_parser("gh-login");
            argparse::ArgumentParser gh_list_parser("gh-list");
            argparse::ArgumentParser delete_repo_parser("delete-repo");
            argparse::ArgumentParser rename_repo_parser("rename-repo");
            program.add_subparser(init_parser);
            program.add_subparser(use_parser);
            program.add_subparser(add_parser);
            program.add_subparser(list_parser);
            program.add_subparser(index_parser);
            program.add_subparser(remove_parser);
            program.add_subparser(rename_parser);
            program.add_subparser(repl_parser);
            program.add_subparser(list_repos_parser);
            program.add_subparser(delete_repo_parser);
            program.add_subparser(rename_repo_parser);
            program.add_subparser(gh_login_parser);
            program.add_subparser(gh_list_parser);
            argparse::ArgumentParser gh_clone_parser("gh-clone");
            argparse::ArgumentParser gh_push_parser("gh-push");
            argparse::ArgumentParser gh_delete_parser("gh-delete");
            argparse::ArgumentParser gh_visibility_parser("gh-visibility");
            program.add_subparser(gh_clone_parser);
            program.add_subparser(gh_push_parser);
            program.add_subparser(gh_delete_parser);
            program.add_subparser(gh_visibility_parser);
            std::cerr << program;
            continue;
        }
        std::vector<std::string> args = parseArgs(line);
        if (args.empty()) continue;

        // Intercept "<subcommand> --help" to avoid parser exiting the REPL
        auto isHelpFlag = [](const std::string& s){ return s == "--help" || s == "-h"; };
        const std::string& sub = args[0];
        bool wantsHelp = false;
        for (size_t i = 1; i < args.size(); ++i) { if (isHelpFlag(args[i])) { wantsHelp = true; break; } }
        if (wantsHelp) {
            if (sub == "init") {
                argparse::ArgumentParser p("init");
                p.add_argument("name").help("repository name").default_value(std::string("MyRepo"));
                p.add_argument("desc").help("optional description").default_value(std::string(""));
                p.add_epilog(
                    "ASCII-only paths required. Юникод в путях не поддерживается.");
                std::cerr << p;
                continue;
            } else if (sub == "use") {
                argparse::ArgumentParser p("use");
                p.add_argument("name").help("repository name");
                p.add_epilog(
                    "ASCII-only paths required. Юникод в путях не поддерживается.");
                std::cerr << p;
                continue;
            } else if (sub == "add") {
                argparse::ArgumentParser p("add");
                p.add_argument("src").help("path to local file (.pk3/.cfg/.exe)");
                p.add_argument("type").help("pk3|cfg|exe").choices("pk3", "cfg", "exe");
                p.add_argument("rel").help("install path relative to repo root (e.g., baseq3/mymap.pk3)");
                p.add_argument("name").help("human-friendly name for the pack/file");
                p.add_argument("--author").help("optional author name").default_value(std::string(""));
                p.add_argument("--desc").help("optional description").default_value(std::string(""));
                p.add_argument("--tag").append().help("add metadata tag (e.g., client:ioq3, mod:osp, category:maps)");
                p.add_epilog(
                    "ASCII-only paths required. Юникод в путях не поддерживается.");
                std::cerr << p;
                continue;
            } else if (sub == "list") {
                argparse::ArgumentParser p("list");
                p.add_epilog(
                    "ASCII-only paths required. Юникод в путях не поддерживается.");
                std::cerr << p;
                continue;
            } else if (sub == "index") {
                argparse::ArgumentParser p("index");
                p.add_epilog(
                    "ASCII-only paths required. Юникод в путях не поддерживается.");
                std::cerr << p;
                continue;
            } else if (sub == "remove") {
                argparse::ArgumentParser p("remove");
                p.add_argument("id").help("item ID to remove");
                p.add_epilog(
                    "ASCII-only paths required. Юникод в путях не поддерживается.");
                std::cerr << p;
                continue;
            } else if (sub == "rename") {
                argparse::ArgumentParser p("rename");
                p.add_argument("id").help("item ID to rename");
                p.add_argument("new_name").help("new name for the item");
                p.add_epilog(
                    "ASCII-only paths required. Юникод в путях не поддерживается.");
                std::cerr << p;
                continue;
            } else if (sub == "list-repos") {
                argparse::ArgumentParser p("list-repos");
                p.add_epilog(
                    "ASCII-only paths required. Юникод в путях не поддерживается.");
                std::cerr << p;
                continue;
            } else if (sub == "delete-repo") {
                argparse::ArgumentParser p("delete-repo");
                p.add_argument("name").help("repository name to delete");
                p.add_argument("--force").help("skip confirmation prompt").default_value(false).implicit_value(true);
                p.add_epilog(
                    "ASCII-only paths required. Юникод в путях не поддерживается.");
                std::cerr << p;
                continue;
            } else if (sub == "rename-repo") {
                argparse::ArgumentParser p("rename-repo");
                p.add_argument("old_name").help("current repository name");
                p.add_argument("new_name").help("new repository name");
                p.add_epilog(
                    "ASCII-only paths required. Юникод в путях не поддерживается.");
                std::cerr << p;
                continue;
            } else if (sub == "gh-login") {
                argparse::ArgumentParser p("gh-login");
                p.add_epilog(
                    "ASCII-only paths required. Юникод в путях не поддерживается.");
                std::cerr << p;
                continue;
            } else if (sub == "gh-list") {
                argparse::ArgumentParser p("gh-list");
                p.add_epilog(
                    "ASCII-only paths required. Юникод в путях не поддерживается.");
                std::cerr << p;
                continue;
            } else if (sub == "gh-clone") {
                argparse::ArgumentParser p("gh-clone");
                p.add_argument("remote").help("owner/repo to clone");
                p.add_argument("--branch").help("branch to use").default_value(std::string("main"));
                p.add_argument("--name").help("local repo name (default repo)").default_value(std::string(""));
                p.add_argument("--set-current").help("set as current repo after clone").default_value(false).implicit_value(true);
                p.add_epilog(
                    "ASCII-only paths required. Юникод в путях не поддерживается.");
                std::cerr << p;
                continue;
            } else if (sub == "gh-push") {
                argparse::ArgumentParser p("gh-push");
                p.add_argument("--remote").help("owner/repo destination").default_value(std::string(""));
                p.add_argument("--branch").help("branch to push").default_value(std::string("main"));
                p.add_argument("--create").help("create repo if missing (user repos)").default_value(false).implicit_value(true);
                p.add_argument("--private").help("create as private when --create").default_value(false).implicit_value(true);
                p.add_argument("--force").help("force push (overwrite remote history)").default_value(false).implicit_value(true);
                p.add_epilog(
                    "ASCII-only paths required. Юникод в путях не поддерживается.");
                std::cerr << p;
                continue;
            } else if (sub == "gh-pull") {
                argparse::ArgumentParser p("gh-pull");
                p.add_argument("--remote").help("owner/repo to pull (default: current repo)").default_value(std::string(""));
                p.add_argument("--branch").help("branch to pull").default_value(std::string("main"));
                p.add_epilog(
                    "ASCII-only paths required. Юникод в путях не поддерживается.");
                std::cerr << p;
                continue;
            } else if (sub == "gh-delete") {
                argparse::ArgumentParser p("gh-delete");
                p.add_argument("remote").help("owner/repo to delete");
                p.add_argument("--force").help("skip confirmation").default_value(false).implicit_value(true);
                p.add_epilog(
                    "ASCII-only paths required. Юникод в путях не поддерживается.");
                std::cerr << p;
                continue;
            } else if (sub == "gh-visibility") {
                argparse::ArgumentParser p("gh-visibility");
                p.add_argument("remote").help("owner/repo to update");
                p.add_argument("visibility").help("public|private").choices("public","private");
                p.add_epilog(
                    "ASCII-only paths required. Юникод в путях не поддерживается.");
                std::cerr << p;
                continue;
            }
        }

        // Build argv
        std::vector<char*> argv_ptr;
        argv_ptr.push_back(const_cast<char*>("repoman-cli")); // Program name
        for (auto& s : args) argv_ptr.push_back(const_cast<char*>(s.c_str()));
        runCommand(static_cast<int>(argv_ptr.size()), argv_ptr.data());
    }
#ifndef _WIN32
    if (!histPath.empty()) utils::saveHistory(histPath, history);
#endif
}

} // namespace cli


