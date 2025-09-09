#include "github_window.h"
#include "imgui.h"
#include <filesystem>
#include <unordered_map>
#include <cstring>
#include <fstream>
#include <iterator>
#include <thread>
#include <algorithm>
#ifdef _WIN32
#include <windows.h>
#endif

#include "system/config.h"
#include "system/version.h"
#include "core/repo.h"
#include "utils/hash.h"
#include "utils/zip.h"

#include "../ui_state.h"
#include "system/fs.h"

namespace ui { namespace menus {

void drawGitHub(State& ui)
{
    if (!ui.showGitHubWindow) return;
    
    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("GitHub Manager", &ui.showGitHubWindow)) {
        auto decodeToken = [&](const std::string& exeDir) -> std::string {
            std::string enc = config::Config::getInstance().getGithubTokenEncrypted();
            if (enc.empty()) return std::string();
            uint8_t key = 0x5A; for (char c : exeDir) key ^= (uint8_t)c; 
            std::string token = enc; 
            for (auto& ch : token) ch = (char)(((uint8_t)ch) ^ key);
            return token;
        };

        // User info and login section
        std::string currentUser = config::Config::getInstance().getGithubUser();
        if (currentUser.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.6f, 1.0f), "Not logged in");
        } else {
            ImGui::TextColored(ImVec4(0.6f, 1.0f, 0.6f, 1.0f), "Logged in as: %s", currentUser.c_str());
        }
        
        ImGui::SameLine();
        if (ImGui::Button("Login")) ImGui::OpenPopup("gh_login");
        if (ImGui::BeginPopupModal("gh_login", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            static char token[256] = {0};
            ImGui::Text("Enter your GitHub personal access token:");
            ImGui::InputText("Token", token, IM_ARRAYSIZE(token), ImGuiInputTextFlags_Password);
            ImGui::Separator();
            if (ImGui::Button("Save & Login")) {
                std::string tmpFile = ui.exeDir + "/gh_user_gui.json";
                std::string cmd = std::string("curl -s -H \"Authorization: token ") + token + "\" https://api.github.com/user -o \"" + tmpFile + "\"";
                int rc = std::system(cmd.c_str()); (void)rc;
                std::ifstream fin(tmpFile);
                std::string content((std::istreambuf_iterator<char>(fin)), std::istreambuf_iterator<char>());
                fin.close(); 
                std::filesystem::remove(tmpFile);
                std::string login; 
                auto pos = content.find("\"login\":");
                if (pos != std::string::npos) { 
                    auto q1 = content.find('"', pos+8); 
                    auto q2 = content.find('"', q1+1); 
                    if (q1!=std::string::npos && q2!=std::string::npos) 
                        login = content.substr(q1+1, q2-q1-1); 
                }
                if (!login.empty()) {
                    uint8_t key = 0x5A; 
                    for (char c : ui.exeDir) key ^= (uint8_t)c; 
                    std::string enc = token; 
                    for (auto& ch : enc) ch = (char)(((uint8_t)ch) ^ key);
                    config::Config::getInstance().setGithubUser(login);
                    config::Config::getInstance().setGithubTokenEncrypted(enc);
                    config::saveConfig(ui.exeDir + "/config.json"); 
                    ui.githubOutput = "Successfully logged in as " + login;
                } else { 
                    ui.githubOutput = "Invalid token or network error"; 
                }
                token[0]=0; 
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine(); 
            if (ImGui::Button("Cancel")) {
                token[0]=0;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::Separator();
        
        // Main GitHub controls with unified remote/branch fields
        ImGui::Text("Repository Settings:");
        ImGui::InputTextWithHint("Remote", "owner/repo (leave empty to auto-detect)", ui.gitHubRemote, IM_ARRAYSIZE(ui.gitHubRemote));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80);
        ImGui::InputTextWithHint("Branch", "main", ui.gitHubBranch, IM_ARRAYSIZE(ui.gitHubBranch));
        
        // For clone operations
        if (ui.currentOperation == Operation::Clone || ui.currentOperation == Operation::None) {
            ImGui::InputTextWithHint("Local Name", "leave empty to use repo name", ui.gitHubLocalName, IM_ARRAYSIZE(ui.gitHubLocalName));
            ImGui::SameLine();
            ImGui::Checkbox("Set as current", &ui.gitHubSetCurrent);
        }
        
        ImGui::Separator();
        
        // Progress bar for ongoing operations
        if (ui.operationInProgress) {
            // Pulse progress if worker hasn't updated it
            ui.operationProgress = std::min(0.98f, ui.operationProgress + 0.003f);
            ImGui::Text("Operation in progress: %s", ui.operationStatus.c_str());
            ImGui::ProgressBar(ui.operationProgress, ImVec2(-1.0f, 0.0f));
            if (ImGui::Button("Cancel")) {
                ui::resetGitHubOperation(ui);
            }
        } else {
            // Main operation buttons
            if (ImGui::Button("List My Repos")) {
                std::string token = decodeToken(ui.exeDir);
                if (token.empty()) { 
                    ui.githubOutput = "Please login first"; 
                } else {
                    ui.currentOperation = Operation::ListRepos;
                    ui.operationInProgress = true;
                    ui.operationStatus = "Fetching repositories...";
                    ui.operationProgress = 0.02f;
                    
                    std::string tmp = ui.exeDir + "/gh_list_gui.json";
                    std::string cmd = std::string("curl -s -H \"Authorization: token ") + token + "\" https://api.github.com/user/repos?per_page=100 -o \"" + tmp + "\"";
                    
                    
                    // Background worker (Windows: Win32 thread; others: std::thread)
                    
#if defined(_WIN32)
                    struct GitHubListTask { ui::State* ui; std::string exeDir; std::string token; std::string tmp; std::string cmd; };
                    auto* task = new GitHubListTask{ &ui, ui.exeDir, token, tmp, cmd };
                    HANDLE h = CreateThread(NULL, 0, [](LPVOID lp)->DWORD {
                        auto* t = static_cast<GitHubListTask*>(lp);
                        std::system(t->cmd.c_str());
                        t->ui->operationStatus = "Parsing repositories...";
                        t->ui->operationProgress = 0.4f;
                        std::ifstream fin(t->tmp);
                        std::string content((std::istreambuf_iterator<char>(fin)), std::istreambuf_iterator<char>());
                        fin.close();
                        std::filesystem::remove(t->tmp);
                        
                        std::vector<ui::GitHubRepoInfo> reposTmp;
                        ui::parseGitHubReposList(content, reposTmp);
                        
                        t->ui->operationStatus = "Checking compatibility...";
                        t->ui->operationProgress = 0.7f;
                        size_t compatibleCount = 0;
                        for (auto &r : reposTmp) {
                            ui::checkRepoCompatibility(t->exeDir, t->token, r);
                            if (r.is_compatible) compatibleCount++;
                        }
                        
                        t->ui->gitHubRepos.swap(reposTmp);
                        t->ui->githubOutput = t->ui->gitHubRepos.empty() ?
                            std::string("No repositories found or API error") :
                            (std::string("Found ") + std::to_string(t->ui->gitHubRepos.size()) + " repositories (" + std::to_string(compatibleCount) + " compatible)");
                        ui::resetGitHubOperation(*t->ui);
                        delete t;
                        return 0;
                    }, task, 0, NULL);
                    if (h) CloseHandle(h);
#else
                    std::thread([&, tmp, cmd, token]() {
                        std::system(cmd.c_str());
                        ui.operationStatus = "Parsing repositories...";
                        ui.operationProgress = 0.4f;
                        std::ifstream fin(tmp);
                        std::string content((std::istreambuf_iterator<char>(fin)), std::istreambuf_iterator<char>());
                        fin.close();
                        std::filesystem::remove(tmp);
                        
                        std::vector<ui::GitHubRepoInfo> reposTmp;
                        ui::parseGitHubReposList(content, reposTmp);
                        
                        ui.operationStatus = "Checking compatibility...";
                        ui.operationProgress = 0.7f;
                        size_t compatibleCount = 0;
                        for (auto &r : reposTmp) {
                            ui::checkRepoCompatibility(ui.exeDir, token, r);
                            if (r.is_compatible) compatibleCount++;
                        }
                        
                        // Publish results
                        ui.gitHubRepos.swap(reposTmp);
                        ui.githubOutput = ui.gitHubRepos.empty() ?
                            std::string("No repositories found or API error") :
                            (std::string("Found ") + std::to_string(ui.gitHubRepos.size()) + " repositories (" + std::to_string(compatibleCount) + " compatible)");
                        ui::resetGitHubOperation(ui);
                    }).detach();
#endif
                }
            }
            
            ImGui::SameLine();
            if (ImGui::Button("Clone")) {
                std::string token = decodeToken(ui.exeDir);
                if (token.empty()) { 
                    ui.githubOutput = "Please login first"; 
                } else if (strlen(ui.gitHubRemote) == 0) {
                    ui.githubOutput = "Please enter remote repository";
                } else {
                    ui.currentOperation = Operation::Clone;
                    ui.operationInProgress = true;
                    ui.operationStatus = "Cloning repository...";
                    ui.operationProgress = 0.3f;
                    
                    std::string remote = ui.gitHubRemote;
                    std::string branch = strlen(ui.gitHubBranch) > 0 ? ui.gitHubBranch : "main";
                    std::string localName = strlen(ui.gitHubLocalName) > 0 ? ui.gitHubLocalName : remote.substr(remote.find('/')+1);
                    std::string repoRoot = ui.exeDir + "/repos/" + localName;
                    
                    if (std::filesystem::exists(repoRoot)) { 
                        ui.githubOutput = " Local repository already exists: " + localName; 
                        ui::resetGitHubOperation(ui);
                    } else {
                        ui.operationProgress = 0.5f;
                        ui.operationStatus = "Downloading archive...";
                        
                        std::string zip = ui.exeDir + "/" + localName + ".zip";
                        std::string url = std::string("https://api.github.com/repos/") + remote + "/zipball/" + branch;
                        std::string cmd = std::string("curl -L -H \"Authorization: token ") + token + "\" -o \"" + zip + "\" \"" + url + "\"";
                        int rc = std::system(cmd.c_str()); (void)rc;
                        
                        ui.operationProgress = 0.7f;
                        ui.operationStatus = "Extracting files...";
                        
                        std::string unzipDir = ui.exeDir + "/ziptmp_" + localName; 
                        std::string err;
                        std::filesystem::create_directories(unzipDir);
                        if (ziputil::extractArchive(zip, unzipDir, err)) {
                            std::string top; 
                            for (auto& e : std::filesystem::directory_iterator(unzipDir)) 
                                if (e.is_directory()) { top = e.path().string(); break; }

                            // Ensure destination parent directory exists (e.g., <exeDir>/repos)
                            std::filesystem::create_directories(std::filesystem::path(repoRoot).parent_path());

                            std::string resultMsg;
                            if (!top.empty()) {
                                std::error_code ec;
                                std::filesystem::rename(top, repoRoot, ec);
                                if (ec) {
                                    resultMsg = std::string(" Clone failed (rename): ") + ec.message();
                                }
                            } else {
                                resultMsg = " Unexpected zip layout (no top-level directory)";
                            }

                            // Cleanup temp files
                            std::filesystem::remove_all(unzipDir); 
                            std::filesystem::remove(zip);

                            if (resultMsg.empty()) {
                                if (ui.gitHubSetCurrent) { 
                                    config::setCurrentRepo(localName); 
                                    config::saveConfig(ui.exeDir + "/config.json"); 
                                    ui.selectedRepo = localName; 
                                }
                                ui::refreshRepos(ui); 
                                ui.githubOutput = " Successfully cloned to repos/" + localName;
                            } else {
                                ui.githubOutput = resultMsg;
                            }
                        } else { 
                            ui.githubOutput = " Extract failed: " + err; 
                        }
                        ui::resetGitHubOperation(ui);
                    }
                }
            }
            
            ImGui::SameLine();
            if (ImGui::Button("Pull")) {
                if (ui.selectedRepo.empty()) {
                    ui.githubOutput = " Please select a local repository first"; 
                } else {
                    std::string token = decodeToken(ui.exeDir);
                    if (token.empty()) { 
                        ui.githubOutput = " Please login first"; 
                    } else {
                        ui.currentOperation = Operation::Pull;
                        ui.operationInProgress = true;
                        ui.operationStatus = "Pulling changes...";
                        ui.operationProgress = 0.3f;
                        
                        std::string remotePath = strlen(ui.gitHubRemote) > 0 ? ui.gitHubRemote : 
                            (currentUser.empty() ? ui.selectedRepo : (currentUser + "/" + ui.selectedRepo));
                        std::string branch = strlen(ui.gitHubBranch) > 0 ? ui.gitHubBranch : "main";
                        
                        std::string zip = ui.exeDir + "/pull_tmp.zip"; 
                        std::string url = "https://api.github.com/repos/" + remotePath + "/zipball/" + branch;
                        std::string cmd = std::string("curl -L -H \"Authorization: token ") + token + "\" -o \"" + zip + "\" \"" + url + "\"";
                        int rc = std::system(cmd.c_str()); (void)rc;
                        
                        ui.operationProgress = 0.7f;
                        ui.operationStatus = "Merging changes...";
                        
                        std::string unzipDir = ui.exeDir + "/pull_unzip_tmp"; 
                        std::string err; 
                        std::filesystem::create_directories(unzipDir);
                        if (ziputil::extractArchive(zip, unzipDir, err)) {
                            std::string top; 
                            for (auto& e : std::filesystem::directory_iterator(unzipDir)) 
                                if (e.is_directory()) { top = e.path().string(); break; }
                            if (!top.empty()) {
                                std::string repoRoot = ui.exeDir + "/repos/" + ui.selectedRepo;
                                for (auto& p : std::filesystem::recursive_directory_iterator(top)) 
                                    if (p.is_regular_file()) {
                                        auto rel = std::filesystem::relative(p.path(), top).string();
                                        std::filesystem::path dest = std::filesystem::path(repoRoot) / rel;
                                        std::filesystem::create_directories(dest.parent_path());
                                        std::error_code ec_rm; std::filesystem::remove(dest, ec_rm);
                                        std::error_code ec_cp; std::filesystem::copy_file(p.path(), dest, std::filesystem::copy_options::overwrite_existing, ec_cp);
                                    }
                            }
                            std::filesystem::remove_all(unzipDir); 
                            std::filesystem::remove(zip); 
                            ui.githubOutput = " Successfully pulled changes from " + remotePath;
                        } else {
                            ui.githubOutput = " Extract failed: " + err;
                        }
                        ui::resetGitHubOperation(ui);
                    }
                }
            }
            
            ImGui::SameLine();
            if (ImGui::Button("Push")) {
                if (ui.selectedRepo.empty()) {
                    ui.githubOutput = " Please select a local repository first"; 
                } else {
                    std::string token = decodeToken(ui.exeDir);
                    if (token.empty()) {
                        ui.githubOutput = " Please login first"; 
                    } else {
                        // Show push options
                        ImGui::OpenPopup("push_options");
                    }
                }
            }
            
            // Push options popup
            if (ImGui::BeginPopupModal("push_options", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("Push Options:");
                ImGui::Separator();
                ImGui::Checkbox("Force push", &ui.gitHubForce);
                ImGui::Checkbox("Create repository if missing", &ui.gitHubCreate);
                if (ui.gitHubCreate) {
                    ImGui::SameLine();
                    ImGui::Checkbox("Private", &ui.gitHubPrivate);
                }
                ImGui::Separator();
                
                if (ImGui::Button("Push Now")) {
                    ui.currentOperation = Operation::Push;
                    ui.operationInProgress = true;
                    ui.operationStatus = "Preparing push...";
                    ui.operationProgress = 0.1f;
                    
                    std::string token = decodeToken(ui.exeDir);
                    std::string repoRoot = ui.exeDir + "/repos/" + ui.selectedRepo;
                    std::string remote = strlen(ui.gitHubRemote) > 0 ? ui.gitHubRemote : ui.selectedRepo;
                    std::string branch = strlen(ui.gitHubBranch) > 0 ? ui.gitHubBranch : "main";
                    bool hasOwner = (remote.find('/') != std::string::npos);
                    std::string remotePath = hasOwner ? remote : (currentUser.empty() ? remote : (currentUser + "/" + remote));
                    
                    // Create repo if needed
                    if (ui.gitHubCreate) {
                        ui.operationStatus = "Creating repository...";
                        ui.operationProgress = 0.3f;
                        
                        std::string body = std::string("{\"name\":\"") + (hasOwner ? remote.substr(remote.find('/')+1) : remote) + 
                                          "\",\"private\":" + (ui.gitHubPrivate ? "true" : "false") + "}";
                        std::string url = hasOwner ? 
                            ("https://api.github.com/orgs/" + remote.substr(0, remote.find('/')) + "/repos") :
                            "https://api.github.com/user/repos";
                        
                        std::string bodyFile = ui.exeDir + "/gh_create_body.json";
                        std::ofstream bf(bodyFile, std::ios::binary);
                        bf << body;
                        bf.close();
                        
                        // Try create at intended URL, capture HTTP code
                        std::string httpCodeFile = ui.exeDir + "/gh_create_code.txt";
                        std::string cmd = std::string("curl -s -o \"") + (ui.exeDir + "/gh_create_resp.json") + "\" -w \"%{http_code}\" -X POST -H \"Authorization: token " + token + 
                                         "\" -H \"Content-Type: application/json\" --data @\"" + bodyFile + "\" \"" + url + "\" > \"" + httpCodeFile + "\"";
                        std::system(cmd.c_str());
                        std::filesystem::remove(bodyFile);
                        
                        // If org creation failed (likely 404/403), retry under user namespace and adjust remotePath
                        if (hasOwner) {
                            std::ifstream codeIn(httpCodeFile);
                            std::string codeStr; codeIn >> codeStr; codeIn.close();
                            std::filesystem::remove(httpCodeFile);
                            if (codeStr != "201" && codeStr != "200") {
                                std::string userUrl = "https://api.github.com/user/repos";
                                std::string cmd2 = std::string("curl -s -X POST -H \"Authorization: token ") + token +
                                                  "\" -H \"Content-Type: application/json\" --data \"" + body + "\" \"" + userUrl + "\"";
                                std::system(cmd2.c_str());
                                if (!currentUser.empty()) {
                                    remotePath = currentUser + "/" + remote.substr(remote.find('/')+1);
                                }
                            }
                        } else {
                            std::filesystem::remove(httpCodeFile);
                        }
                    }
                    
                    ui.operationStatus = "Pushing changes...";
                    ui.operationProgress = 0.7f;
                    
                    // Git operations
                    std::string https = std::string("https://") + token + "@github.com/" + remotePath + ".git";
                    auto gitCmd = [&](const std::string& c) { 
                        return std::string("cd \"") + repoRoot + "\" && " + c; 
                    };
                    
#ifdef _WIN32
                    const char* redirectStderrToNull = " 2>nul";
#else
                    const char* redirectStderrToNull = " 2>/dev/null";
#endif
                    
                    // Set local identity to avoid leaking host username/hostname
                    std::string ghUser = config::Config::getInstance().getGithubUser();
                    if (ghUser.empty()) ghUser = "github-user";
                    std::string appSig = std::string(appinfo::kAppName) + "/" + appinfo::kAppVersion;
                    std::system(gitCmd(std::string("git config user.name \"") + ghUser + " (" + appSig + ")\"").c_str());
                    std::system(gitCmd(std::string("git config user.email \"") + ghUser + "@users.noreply.github.com\"").c_str());
                    
                    std::system(gitCmd("git init").c_str());
                    std::system(gitCmd(std::string("git remote remove origin") + redirectStderrToNull).c_str());
                    std::system(gitCmd(std::string("git remote add origin \"") + https + "\"").c_str());
                    std::system(gitCmd(std::string("git checkout -B ") + branch).c_str());
                    std::system(gitCmd("git add .").c_str());
                    std::system(gitCmd(std::string("git commit -m \"Update via RepoMan GUI\"") + redirectStderrToNull).c_str());
                    int pushResult = std::system(gitCmd(std::string("git push ") + (ui.gitHubForce ? "-f " : "") + "-u origin " + branch + redirectStderrToNull).c_str());
                    
                    if (pushResult == 0) {
                        ui.githubOutput = " Successfully pushed to " + remotePath + " (" + branch + ")";
                    } else {
                        ui.githubOutput = " Push failed. Check repository permissions.";
                    }
                    
                    ui::resetGitHubOperation(ui);
                    ImGui::CloseCurrentPopup();
                }
                
                ImGui::SameLine();
                if (ImGui::Button("Cancel")) {
                    ImGui::CloseCurrentPopup();
                }
                
                ImGui::EndPopup();
            }
        }

        ImGui::Separator();
        
        // Repository list display (human-readable)
        if (!ui.gitHubRepos.empty()) {
            static bool showOnlyCompatible = true;
            ImGui::Text("Your Repositories:");
            ImGui::SameLine();
            ImGui::Checkbox("Show only compatible", &showOnlyCompatible);
            
            // Filter repositories based on compatibility
            std::vector<std::reference_wrapper<const ui::GitHubRepoInfo>> filteredRepos;
            for (const auto& repo : ui.gitHubRepos) {
                if (!showOnlyCompatible || repo.is_compatible) {
                    filteredRepos.push_back(std::cref(repo));
                }
            }
            
            ImGui::Text("Showing %zu repositories", filteredRepos.size());
            
            if (!filteredRepos.empty() && ImGui::BeginTable("github_repos", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                ImGui::TableSetupColumn("Private", ImGuiTableColumnFlags_WidthFixed, 60.0f);
                ImGui::TableSetupColumn("Local", ImGuiTableColumnFlags_WidthFixed, 70.0f);
                ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableHeadersRow();
                
                for (size_t i = 0; i < filteredRepos.size(); ++i) {
                    const auto& repo = filteredRepos[i].get();
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    std::string selectableId = repo.full_name + "##filtered_repo_" + std::to_string(i);
                    
                    // Only allow selection of compatible repositories
                    bool canSelect = repo.is_compatible;
                    if (canSelect && ImGui::Selectable(selectableId.c_str(), false, ImGuiSelectableFlags_SpanAllColumns)) {
                        // Auto-fill the remote field when clicking on a repo
                        std::snprintf(ui.gitHubRemote, sizeof(ui.gitHubRemote), "%s", repo.full_name.c_str());
                    } else if (!canSelect) {
                        // Show incompatible repos as disabled
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
                        ImGui::Selectable(selectableId.c_str(), false, ImGuiSelectableFlags_Disabled | ImGuiSelectableFlags_SpanAllColumns);
                        ImGui::PopStyleColor();
                    }

                    // Context menu for GitHub repo row
                    if (ImGui::BeginPopupContextItem(("gh_repo_ctx_" + selectableId).c_str())) {
                        ImGui::TextUnformatted(repo.full_name.c_str());
                        ImGui::Separator();
                        if (ImGui::MenuItem("Use as Remote")) {
                            std::snprintf(ui.gitHubRemote, sizeof(ui.gitHubRemote), "%s", repo.full_name.c_str());
                        }
                        if (ImGui::MenuItem("Open in Browser")) {
                            #ifdef _WIN32
                            std::string cmd = std::string("start \"") + repo.html_url + "\"";
                            #else
                            std::string cmd = std::string("xdg-open \"") + repo.html_url + "\"";
                            #endif
                            std::system(cmd.c_str());
                        }
                        if (ImGui::MenuItem("Rename Repository")) {
                            std::snprintf(ui.githubRenameBuffer, sizeof(ui.githubRenameBuffer), "%s", repo.name.c_str());
                            std::snprintf(ui.gitHubSelectedFullName, sizeof(ui.gitHubSelectedFullName), "%s", repo.full_name.c_str());
                            ImGui::OpenPopup("rename_github_repo");
                        }
                        if (ImGui::MenuItem(repo.is_private ? "Make Public" : "Make Private")) {
                            std::string token = decodeToken(ui.exeDir);
                            if (!token.empty()) {
                                std::string body = std::string("{\"private\":") + (repo.is_private ? "false" : "true") + "}";
                                std::string bodyFile = ui.exeDir + "/gh_vis_body_ctx.json";
                                std::ofstream bf(bodyFile);
                                bf << body;
                                bf.close();
                                std::string cmd = std::string("curl -s -X PATCH -H \"Authorization: token ") + token +
                                                 "\" -H \"Content-Type: application/json\" --data @\"" + bodyFile +
                                                 "\" \"https://api.github.com/repos/" + repo.full_name + "\"";
                                int result = std::system(cmd.c_str());
                                std::filesystem::remove(bodyFile);
                                ui.githubOutput = result == 0 ? (std::string("Visibility updated for ") + repo.full_name)
                                                              : (std::string("Failed to update visibility for ") + repo.full_name);
                                // Fetch fresh repo info and update row
                                if (result == 0) {
                                    std::string tmp = ui.exeDir + "/gh_repo_refresh.json";
                                    std::string getCmd = std::string("curl -s -H \"Authorization: token ") + token +
                                                         "\" \"https://api.github.com/repos/" + repo.full_name + "\" -o \"" + tmp + "\"";
                                    std::system(getCmd.c_str());
                                    std::ifstream fin(tmp);
                                    std::string content((std::istreambuf_iterator<char>(fin)), std::istreambuf_iterator<char>());
                                    fin.close();
                                    std::filesystem::remove(tmp);
                                    // Minimal parse of 'private' and 'description'
                                    auto find_bool = [&](const std::string& key)->bool { auto p = content.find(key); if(p==std::string::npos) return false; auto q = content.find_first_of("tf", p); return q!=std::string::npos && content.substr(q,4)=="true"; };
                                    auto find_string = [&](const std::string& key)->std::string { auto p=content.find(key); if(p==std::string::npos) return std::string(); auto q1=content.find('"', p+key.size()); if(q1==std::string::npos) return std::string(); auto q2=content.find('"', q1+1); if(q2==std::string::npos) return std::string(); return content.substr(q1+1, q2-q1-1); };
                                    for (auto &r : ui.gitHubRepos) {
                                        if (r.full_name == repo.full_name) {
                                            r.is_private = find_bool("\"private\":");
                                            std::string desc = find_string("\"description\":");
                                            if (!desc.empty()) r.description = desc;
                                            break;
                                        }
                                    }
                                }
                            } else {
                                ui.githubOutput = "Please login first";
                            }
                        }
                        if (ImGui::MenuItem("Delete Repository")) {
                            std::string token = decodeToken(ui.exeDir);
                            if (!token.empty()) {
                                std::string cmd = std::string("curl -s -X DELETE -H \"Authorization: token ") + token +
                                                 "\" \"https://api.github.com/repos/" + repo.full_name + "\"";
                                int result = std::system(cmd.c_str());
                                ui.githubOutput = result == 0 ? (std::string("Repository deleted: ") + repo.full_name)
                                                              : (std::string("Failed to delete repository: ") + repo.full_name);
                                if (result == 0) {
                                    // Remove from table immediately
                                    ui.gitHubRepos.erase(std::remove_if(ui.gitHubRepos.begin(), ui.gitHubRepos.end(), [&](const ui::GitHubRepoInfo& r){return r.full_name==repo.full_name;}), ui.gitHubRepos.end());
                                }
                            } else {
                                ui.githubOutput = "Please login first";
                            }
                        }
                        if (ImGui::MenuItem("Copy Name")) {
                            ImGui::SetClipboardText(repo.full_name.c_str());
                        }
                        ImGui::EndPopup();
                    }
                    
                    ImGui::TableSetColumnIndex(1);
                    if (repo.is_compatible) {
                        ImGui::TextColored(ImVec4(0.0f, 0.8f, 0.0f, 1.0f), "Compatible");
                    } else {
                        ImGui::TextColored(ImVec4(0.8f, 0.4f, 0.0f, 1.0f), "Incompatible");
                    }
                    
                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text(repo.is_private ? "Private" : "Public");
                    
                    // Local presence
                    ImGui::TableSetColumnIndex(3);
                    {
                        std::string localRoot = ui.exeDir + std::string("/repos/") + (repo.full_name.find('/')!=std::string::npos ? repo.full_name.substr(repo.full_name.find('/')+1) : repo.full_name);
                        bool exists = std::filesystem::exists(localRoot);
                        ImGui::TextColored(exists ? ImVec4(0.5f,1.0f,0.5f,1.0f) : ImVec4(1.0f,0.6f,0.6f,1.0f), exists?"Yes":"No");
                    }
                    ImGui::TableSetColumnIndex(4);
                    ImGui::TextWrapped("%s", repo.description.c_str());
                }
                ImGui::EndTable();
            }
            ImGui::Separator();
            // Inline popup for rename
            if (ImGui::BeginPopupModal("rename_github_repo", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("Rename GitHub repository:");
                ImGui::InputText("New name", ui.githubRenameBuffer, IM_ARRAYSIZE(ui.githubRenameBuffer));
                if (ImGui::Button("Apply")) {
                    std::string token = decodeToken(ui.exeDir);
                    if (!token.empty() && strlen(ui.gitHubSelectedFullName) > 0) {
                        // PATCH repo name
                        std::string owner_repo = ui.gitHubSelectedFullName;
                        std::string bodyFile = ui.exeDir + "/gh_rename_body.json";
                        std::ofstream bf(bodyFile);
                        bf << std::string("{\"name\":\"") << ui.githubRenameBuffer << "\"}";
                        bf.close();
                        std::string cmd = std::string("curl -s -X PATCH -H \"Authorization: token ") + token +
                                         "\" -H \"Content-Type: application/json\" --data @\"" + bodyFile +
                                         "\" \"https://api.github.com/repos/" + owner_repo + "\"";
                        std::system(cmd.c_str());
                        std::filesystem::remove(bodyFile);
                        // Fetch fresh data and update row
                        std::string tmp = ui.exeDir + "/gh_repo_refresh.json";
                        std::string getCmd = std::string("curl -s -H \"Authorization: token ") + token +
                                             "\" \"https://api.github.com/repos/" + owner_repo + "\" -o \"" + tmp + "\"";
                        std::system(getCmd.c_str());
                        std::ifstream fin(tmp);
                        std::string content((std::istreambuf_iterator<char>(fin)), std::istreambuf_iterator<char>());
                        fin.close();
                        std::filesystem::remove(tmp);
                        // Parse name/full_name/description/private
                        auto find_string = [&](const std::string& key)->std::string { auto p=content.find(key); if(p==std::string::npos) return std::string(); auto q1=content.find('"', p+key.size()); if(q1==std::string::npos) return std::string(); auto q2=content.find('"', q1+1); if(q2==std::string::npos) return std::string(); return content.substr(q1+1, q2-q1-1); };
                        auto find_bool = [&](const std::string& key)->bool { auto p=content.find(key); if(p==std::string::npos) return false; auto q=content.find_first_of("tf", p); return q!=std::string::npos && content.substr(q,4)=="true"; };
                        std::string new_name = find_string("\"name\":");
                        std::string new_full = find_string("\"full_name\":");
                        std::string new_desc = find_string("\"description\":");
                        bool new_priv = find_bool("\"private\":");
                        for (auto &r : ui.gitHubRepos) {
                            if (r.full_name == owner_repo) {
                                if (!new_name.empty()) r.name = new_name;
                                if (!new_full.empty()) r.full_name = new_full;
                                if (!new_desc.empty()) r.description = new_desc;
                                r.is_private = new_priv;
                                break;
                            }
                        }
                        ui.githubOutput = "Repository renamed.";
                    }
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel")) { ImGui::CloseCurrentPopup(); }
                ImGui::EndPopup();
            }
        }
        
        // Status/output area
        ImGui::Text("Status:");
        ImGui::BeginChild("gh_output", ImVec2(0, 80), true);
        if (!ui.githubOutput.empty()) {
            ImGui::TextWrapped("%s", ui.githubOutput.c_str());
        } else {
            ImGui::TextDisabled("Ready");
        }
        ImGui::EndChild();
        
        // Advanced operations in a separate section
        if (ImGui::CollapsingHeader("Advanced Operations")) {
            ImGui::InputTextWithHint("Repository to manage", "owner/repo", ui.gitHubRemote, IM_ARRAYSIZE(ui.gitHubRemote));
            
            if (ImGui::Button("Delete Repository")) {
                ImGui::OpenPopup("confirm_delete");
            }
            
            if (ImGui::BeginPopupModal("confirm_delete", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("Are you sure you want to DELETE this repository?");
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", ui.gitHubRemote);
                ImGui::Text("This action cannot be undone!");
                ImGui::Separator();
                
                if (ImGui::Button("Yes, DELETE")) {
                    std::string token = decodeToken(ui.exeDir);
                    if (!token.empty() && strlen(ui.gitHubRemote) > 0) {
                        std::string cmd = std::string("curl -s -X DELETE -H \"Authorization: token ") + token + 
                                         "\" \"https://api.github.com/repos/" + ui.gitHubRemote + "\"";
                        int result = std::system(cmd.c_str());
                        ui.githubOutput = result == 0 ? (" Repository " + std::string(ui.gitHubRemote) + " deleted") :
                                                       (" Failed to delete repository");
                    } else {
                        ui.githubOutput = " Please login and specify repository";
                    }
                    ImGui::CloseCurrentPopup();
                }
                
                ImGui::SameLine();
                if (ImGui::Button("Cancel")) {
                    ImGui::CloseCurrentPopup();
                }
                
                ImGui::EndPopup();
            }
            
            ImGui::SameLine();
            ImGui::SetNextItemWidth(100);
            const char* visOptions[] = {"Public", "Private"};
            ImGui::Combo("Visibility", &ui.gitHubVisibility, visOptions, 2);
            ImGui::SameLine();
            if (ImGui::Button("Update Visibility")) {
                std::string token = decodeToken(ui.exeDir);
                if (!token.empty() && strlen(ui.gitHubRemote) > 0) {
                    std::string body = std::string("{\"private\":") + (ui.gitHubVisibility == 1 ? "true" : "false") + "}";
                    std::string bodyFile = ui.exeDir + "/gh_vis_body.json";
                    std::ofstream bf(bodyFile);
                    bf << body;
                    bf.close();
                    
                    std::string cmd = std::string("curl -s -X PATCH -H \"Authorization: token ") + token + 
                                     "\" -H \"Content-Type: application/json\" --data @\"" + bodyFile + 
                                     "\" \"https://api.github.com/repos/" + ui.gitHubRemote + "\"";
                    int result = std::system(cmd.c_str());
                    std::filesystem::remove(bodyFile);
                    
                    ui.githubOutput = result == 0 ? (" Visibility updated to " + std::string(visOptions[ui.gitHubVisibility])) :
                                                   (" Failed to update visibility");
                } else {
                    ui.githubOutput = " Please login and specify repository";
                }
            }
        }
    
    ImGui::End();
}
}

} } // namespace ui::menus
