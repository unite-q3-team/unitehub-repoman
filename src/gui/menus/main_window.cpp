#include "imgui.h"
#include <filesystem>
#include <unordered_map>
#include <cstring>
#include <fstream>
#include <iterator>
#include "system/config.h"
#include "core/repo.h"
#include "utils/hash.h"
#include "utils/zip.h"
#include "../ui_state.h"
#include "github_window.h"
#include <nlohmann/json.hpp>
#include <ctime>
#include <algorithm>
#ifdef _WIN32
#include <windows.h>
#endif
namespace ui { namespace menus {
void drawMain(State& ui, bool* p_open)
{
    static bool firstFrame = true;
    // Auto-resize window based on content
    if (!ui.selectedRepo.empty()) {
        std::string repoRoot = ui.exeDir + "/repos/" + ui.selectedRepo;
        core::RepoManager repo(repoRoot);
        repo.loadIndex();
        const auto& items = repo.index().items;
        // Calculate appropriate window height
        float baseHeight = 300; // Minimum height for UI elements
        float itemsHeight = (float)items.size() * 25.0f + 60.0f; // 25px per row + header
        float githubHeight = ui.gitHubRepos.empty() ? 0 : std::min(200.0f, (float)ui.gitHubRepos.size() * 20.0f + 100.0f);
        float totalHeight = baseHeight + itemsHeight + githubHeight;
        // Clamp to reasonable limits
        totalHeight = std::max(400.0f, std::min(totalHeight, 800.0f));
        ImGui::SetNextWindowSize(ImVec2(900, totalHeight), ImGuiCond_FirstUseEver);
    } else if (firstFrame) {
        ImGui::SetNextWindowSize(ImVec2(700, 400), ImGuiCond_FirstUseEver);
        firstFrame = false;
    }
    if (!ImGui::Begin("RepoMan", p_open, ImGuiWindowFlags_NoCollapse)) { ImGui::End(); return; }
    if (ImGui::BeginCombo("Repository", ui.selectedRepo.empty() ? "<none>" : ui.selectedRepo.c_str())) {
        for (auto& name : ui.repoNames) {
            bool selected = (name == ui.selectedRepo);
            if (ImGui::Selectable(name.c_str(), selected)) {
                ui.selectedRepo = name;
                config::setCurrentRepo(name);
                config::saveConfig(ui.exeDir + "/config.json");
                ui.selectedItemIndex = -1; ui.selectedItemId.clear();
                // Auto-fill GitHub remote as owner/repo when possible
                std::string ghUser = config::Config::getInstance().getGithubUser();
                if (!ghUser.empty()) {
                    std::snprintf(ui.gitHubRemote, IM_ARRAYSIZE(ui.gitHubRemote), "%s/%s", ghUser.c_str(), name.c_str());
                }
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    // Repository actions moved to top menu bar
    static char initName[128] = {0};
    static char initDesc[256] = {0};
    if (ImGui::BeginPopupModal("init_repo", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("Name", initName, IM_ARRAYSIZE(initName));
        ImGui::InputText("Description", initDesc, IM_ARRAYSIZE(initDesc));
        if (ImGui::Button("Create")) {
            std::string root = ui.exeDir + "/repos/" + std::string(initName);
            core::RepoManager repo(root);
            if (repo.init(initName, initDesc)) {
                ui::refreshRepos(ui);
                ui.selectedRepo = initName;
                config::setCurrentRepo(ui.selectedRepo);
                config::saveConfig(ui.exeDir + "/config.json");
                initName[0]=0; initDesc[0]=0;
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) { initName[0]=0; initDesc[0]=0; ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
    ImGui::Separator();
    if (!ui.selectedRepo.empty()) {
        std::string repoRoot = ui.exeDir + "/repos/" + ui.selectedRepo;
        core::RepoManager repo(repoRoot);
        repo.loadIndex();
        // Item actions moved to top menu bar
        if (false) {
            size_t missing=0, hashMismatch=0, dupPaths=0, dupIds=0;
            std::unordered_map<std::string,int> pathCount, idCount;
            for (const auto& it : repo.index().items) { pathCount[it.relativePath]++; idCount[it.id]++; }
            for (const auto& it : repo.index().items) {
                std::string full = repoRoot + "/" + it.relativePath;
                if (!std::filesystem::exists(full)) { ++missing; continue; }
                if (!it.sha256.empty()) {
                    std::string got = utils::computeFileSha256(full);
                    if (!got.empty() && got != it.sha256) ++hashMismatch;
                }
            }
            for (auto& kv : pathCount) if (kv.second>1) dupPaths += kv.second-1;
            for (auto& kv : idCount) if (kv.second>1) dupIds += kv.second-1;
            ImGui::OpenPopup("verify_popup");
        }
        // Rename/Delete moved to top menu bar
        if (ui.confirmDeleteRepo) ImGui::OpenPopup("confirm_delete_local_repo");
        if (ImGui::BeginPopupModal("confirm_delete_local_repo", &ui.confirmDeleteRepo, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Delete local repository '%s'?", ui.selectedRepo.c_str());
            ImGui::TextColored(ImVec4(1.0f,0.5f,0.5f,1.0f), "This will permanently remove all files.");
            ImGui::Separator();
            if (ImGui::Button("Yes, delete")) {
                std::string repoRootDel = ui.exeDir + "/repos/" + ui.selectedRepo;
                std::error_code ec; std::filesystem::remove_all(repoRootDel, ec);
                ui.selectedRepo.clear();
                config::setCurrentRepo("");
                config::saveConfig(ui.exeDir + "/config.json");
                ui::refreshRepos(ui);
                ui.confirmDeleteRepo = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) { ui.confirmDeleteRepo = false; ImGui::CloseCurrentPopup(); }
            ImGui::EndPopup();
        }
        // Rename repository modal
        if (ui.showRenameRepoModal) ImGui::OpenPopup("rename_local_repo");
        if (ImGui::BeginPopupModal("rename_local_repo", &ui.showRenameRepoModal, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Rename repository:");
            ImGui::InputText("New name", ui.renameRepoBuffer, IM_ARRAYSIZE(ui.renameRepoBuffer));
            ImGui::Separator();
            if (ImGui::Button("Rename") && strlen(ui.renameRepoBuffer) > 0) {
                std::string oldName = ui.selectedRepo;
                std::string newName = ui.renameRepoBuffer;
                std::string oldPath = ui.exeDir + "/repos/" + oldName;
                std::string newPath = ui.exeDir + "/repos/" + newName;
                if (!std::filesystem::exists(newPath)) {
                    std::error_code ec;
                    std::filesystem::rename(oldPath, newPath, ec);
                    if (!ec) {
                        ui.selectedRepo = newName;
                        config::setCurrentRepo(newName);
                        config::saveConfig(ui.exeDir + "/config.json");
                        ui::refreshRepos(ui);
                    }
                }
                ui.showRenameRepoModal = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                ui.showRenameRepoModal = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        if (ImGui::BeginPopupModal("verify_popup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            // Moved verification logic here to ensure repo is in scope
            size_t missing=0, hashMismatch=0, dupPaths=0, dupIds=0;
            std::unordered_map<std::string,int> pathCount, idCount;
            for (const auto& it : repo.index().items) { pathCount[it.relativePath]++; idCount[it.id]++; }
            for (const auto& it : repo.index().items) {
                std::string full = repoRoot + "/" + it.relativePath;
                if (!std::filesystem::exists(full)) { ++missing; continue; }
                if (!it.sha256.empty()) {
                    std::string got = utils::computeFileSha256(full);
                    if (!got.empty() && got != it.sha256) ++hashMismatch;
                }
            }
            for (auto& kv : pathCount) if (kv.second>1) dupPaths += kv.second-1;
            for (auto& kv : idCount) if (kv.second>1) dupIds += kv.second-1;
            ImGui::Text("Missing: %zu\nHash mismatch: %zu\nDup paths: %zu\nDup ids: %zu", missing, hashMismatch, dupPaths, dupIds);
            if (ImGui::Button("Close")) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
        if (ui.showAddModal) ImGui::OpenPopup("add_item");
        if (ImGui::BeginPopupModal("add_item", &ui.showAddModal, ImGuiWindowFlags_AlwaysAutoResize)) {
            // If a single file was dropped, auto-populate src/name/type (do NOT auto-fill relative path)
            if (ui.dropQueue.size() == 1) {
                const std::string& src = ui.dropQueue.front();
                std::snprintf(ui.addSrc, IM_ARRAYSIZE(ui.addSrc), "%s", src.c_str());
                // Infer rel and name
                std::filesystem::path sp(src);
                std::string filename = sp.filename().string();
                std::string stem = sp.stem().string();
                if (ui.addName[0] == '\0') std::snprintf(ui.addName, IM_ARRAYSIZE(ui.addName), "%s", stem.c_str());
                // Infer type from extension
                std::string ext = sp.extension().string();
                for (auto& c : ext) c = (char)tolower((unsigned char)c);
                if (ext == ".cfg") ui.addType = 1; else if (ext == ".exe") ui.addType = 2; else ui.addType = 0;
                ui.dropQueue.clear();
            }
            // If multiple files dropped, hide single Source path and show include checklist
            bool multiple = ui.dropQueue.size() > 1;
            if (!multiple) {
                ImGui::InputText("Source path", ui.addSrc, IM_ARRAYSIZE(ui.addSrc));
            } else {
                if (ui.dropInclude.size() != ui.dropQueue.size()) ui.dropInclude.assign(ui.dropQueue.size(), true);
                ImGui::TextUnformatted("Dropped files (uncheck to exclude):");
                ImGui::BeginChild("dropped", ImVec2(520, 160), true);
                for (size_t i = 0; i < ui.dropQueue.size(); ++i) {
                    bool inc = (i < ui.dropInclude.size()) ? (bool)ui.dropInclude[i] : true;
                    if (ImGui::Checkbox(std::string("##inc_" + std::to_string(i)).c_str(), &inc)) {
                        if (i < ui.dropInclude.size()) ui.dropInclude[i] = inc;
                    }
                    ImGui::SameLine();
                    ImGui::TextWrapped("%s", ui.dropQueue[i].c_str());
                }
                ImGui::EndChild();
            }
            const char* types[] = {"pk3","cfg","exe"};
            ImGui::Combo("Type", &ui.addType, types, 3);
            ImGui::InputText("Relative path", ui.addRel, IM_ARRAYSIZE(ui.addRel));
            ImGui::InputText("Name", ui.addName, IM_ARRAYSIZE(ui.addName));
            ImGui::InputText("Author", ui.addAuthor, IM_ARRAYSIZE(ui.addAuthor));
            ImGui::InputText("Description", ui.addDesc, IM_ARRAYSIZE(ui.addDesc));
            // Tags management with key:value pairs (same as Edit Metadata)
            ImGui::Text("Tags (key:value format):");
            ImGui::BeginChild("add_tags_list", ImVec2(480, 100), true);
            for (size_t ti = 0; ti < ui.addTagsList.size(); ++ti) {
                ImGui::PushID((int)ti);
                const auto& tagPair = ui.addTagsList[ti];
                if (tagPair.second.empty()) {
                    ImGui::Text("- %s", tagPair.first.c_str());
                } else {
                    ImGui::Text("- %s:%s", tagPair.first.c_str(), tagPair.second.c_str());
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Remove")) {
                    ui.addTagsList.erase(ui.addTagsList.begin() + ti);
                    --ti; // Adjust index after removal
                }
                ImGui::PopID();
            }
            ImGui::EndChild();
            // Add new tag with key:value format
            ImGui::Text("Add new tag:");
            ImGui::InputText("Key##add", ui.addNewTagKey, IM_ARRAYSIZE(ui.addNewTagKey));
            ImGui::SameLine();
            ImGui::InputText("Value##add", ui.addNewTagValue, IM_ARRAYSIZE(ui.addNewTagValue));
            ImGui::SameLine();
            if (ImGui::Button("Add Tag") && strlen(ui.addNewTagKey) > 0) {
                std::string key = ui.addNewTagKey;
                std::string value = ui.addNewTagValue;
                // Check if key already exists
                bool exists = false;
                for (const auto& tagPair : ui.addTagsList) {
                    if (tagPair.first == key) {
                        exists = true;
                        break;
                    }
                }
                if (!exists) {
                    ui.addTagsList.push_back(std::make_pair(key, value)); // Fixed brace-init-list issue
                }
                ui.addNewTagKey[0] = 0;
                ui.addNewTagValue[0] = 0;
            }
            if (ImGui::Button("Add")) {
                // Convert key:value pairs to string format
                std::vector<std::string> tags;
                for (const auto& tagPair : ui.addTagsList) {
                    if (tagPair.second.empty()) {
                        tags.push_back(tagPair.first);
                    } else {
                        tags.push_back(tagPair.first + ":" + tagPair.second);
                    }
                }
                std::vector<std::string> files = ui.dropQueue.empty() ? std::vector<std::string>{ui.addSrc} : ui.dropQueue;
                for (size_t idx=0; idx<files.size(); ++idx) {
                    if (!ui.dropQueue.empty() && idx < ui.dropInclude.size() && !ui.dropInclude[idx]) continue;
                    const std::string& src = files[idx];
                    core::ContentType t = core::ContentType::PK3;
                    if (ui.addType == 1) t = core::ContentType::CFG; else if (ui.addType == 2) t = core::ContentType::EXECUTABLE;
                    std::filesystem::path sp(src);
                    std::string rel = ui.addRel;
                    if (files.size() > 1) {
                        // Multiple import: if rel empty -> root/filename; if rel ends with '/' -> prefix+filename; else treat rel as folder
                        if (rel.empty()) rel = sp.filename().string();
                        else if (rel.back() == '/') rel += sp.filename().string();
                        else rel = rel + "/" + sp.filename().string();
                    } else {
                        // Single import: if rel empty -> filename at repo root; if rel ends with '/' -> prefix+filename; else use as-is
                        if (rel.empty()) rel = sp.filename().string();
                        else if (rel.back() == '/') rel += sp.filename().string();
                    }
                    // If item with same rel exists, we update by re-importing and overwriting
                    core::RepoManager addRepo(repoRoot); // Declare repo manager inside loop scope
                    addRepo.loadIndex();
                    auto id = addRepo.addFile(src, t, rel, ui.addName, ui.addDesc, ui.addAuthor, tags, ""); (void)id;
                }
                ui.showAddModal = false;
                ui.addSrc[0]=ui.addRel[0]=ui.addName[0]=ui.addAuthor[0]=ui.addDesc[0]=ui.addTags[0]=0;
                ui.addType=0;
                ui.addTagsList.clear();
                ui.addNewTagKey[0] = ui.addNewTagValue[0] = 0;
                ui.dropQueue.clear();
                ui.dropInclude.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Close")) {
                ui.showAddModal = false;
                // Clear fields so old values don't persist next time
                ui.addSrc[0]=ui.addRel[0]=ui.addName[0]=ui.addAuthor[0]=ui.addDesc[0]=ui.addTags[0]=0;
                ui.addType = 0;
                ui.addTagsList.clear();
                ui.addNewTagKey[0] = ui.addNewTagValue[0] = 0;
                ui.dropQueue.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        ImGui::Separator();
        // Show GitHub operations progress in main view as well
        if (ui.operationInProgress || ui.gitHubCompareInProgress) {
            ImGui::TextUnformatted(ui.operationInProgress ? ui.operationStatus.c_str() : "Comparing with GitHub...");
            float prog = ui.operationInProgress ? ui.operationProgress : 0.5f;
            ImGui::ProgressBar(prog, ImVec2(-1.0f, 0.0f));
            if (ui.operationInProgress) {
                if (ImGui::SmallButton("Cancel Operation")) {
                    ui::resetGitHubOperation(ui);
                }
            }
            ImGui::Separator();
        }
        // Handle rescan request from top menu
        if (ui.requestRescan) {
            repo.pruneMissingFiles();
            repo.discoverNewFiles();
            repo.loadIndex();
            ui.requestRescan = false;
        }
        // Filters moved to top menu bar; show count only
        ImGui::Text("Repository Items (%d):", (int)repo.index().items.size()); // repo now in scope
        ImGui::SameLine();
        ImGui::SetNextItemWidth(240);
        ImGui::InputTextWithHint("##live_search", "Search name or path...", ui.filterSearch, IM_ARRAYSIZE(ui.filterSearch));
        // Batch actions
        ImGui::Separator();
        int selectedCount = (int)ui.selectedItemIdsSet.size();
        ImGui::BeginDisabled(selectedCount == 0);
        if (ImGui::Button("Batch Edit Tags")) { ui.showBatchTagsModal = true; }
        ImGui::SameLine();
        if (ImGui::Button("Batch Move")) { ui.showBatchMoveModal = true; }
        ImGui::SameLine();
        if (ImGui::Button("Batch Delete")) { ImGui::OpenPopup("batch_delete_popup"); }
        ImGui::EndDisabled();
        if (selectedCount > 0) { ImGui::SameLine(); ImGui::Text("Selected: %d", selectedCount); }
        ImGui::Separator();
        ImGui::SameLine();
        if (ImGui::SmallButton("Compare with GitHub")) {
            // Load remote index.json into a set of relative paths
            ui.gitHubCompareReady = false;
            ui.gitHubCompareInProgress = true;
            ui.gitHubRemotePaths.clear();
            ui.gitHubRemoteOnlyCount = 0;
            ui.gitHubRemoteOnlySample.clear();
            // Get token for private repos
            auto decodeToken = [&](const std::string& exeDir) -> std::string {
                std::string enc = config::Config::getInstance().getGithubTokenEncrypted();
                if (enc.empty()) return std::string();
                uint8_t key = 0x5A; for (char c : exeDir) key ^= (uint8_t)c;
                std::string token = enc;
                for (auto& ch : token) ch = (char)(((uint8_t)ch) ^ key);
                return token;
            };
            std::string token = decodeToken(ui.exeDir);
            // Determine remote path guess
            std::string remotePath = strlen(ui.gitHubRemote)>0 ? ui.gitHubRemote : (config::Config::getInstance().getGithubUser().empty()?ui.selectedRepo:(config::Config::getInstance().getGithubUser()+"/"+ui.selectedRepo));
            std::string branch = strlen(ui.gitHubBranch)>0 ? ui.gitHubBranch : "main";
            std::string tmpIdx = ui.exeDir + "/gh_idx_compare.json";
            // Try API first (works for private repos), then fallback to raw
            std::string apiUrl = std::string("https://api.github.com/repos/") + remotePath + "/contents/index.json?ref=" + branch;
            std::string cmd;
            if (!token.empty()) {
                // Request raw content directly for private repos
                cmd = std::string("curl -sL -H \"Authorization: token ") + token +
                      "\" -H \"Accept: application/vnd.github.v3.raw\" \"" + apiUrl + "\" -o \"" + tmpIdx + "\"";
            } else {
                std::string rawUrl = std::string("https://raw.githubusercontent.com/") + remotePath + "/" + branch + "/index.json";
                cmd = std::string("curl -sL \"") + rawUrl + "\" -o \"" + tmpIdx + "\"";
            }
            // Execute synchronously to avoid MinGW threading issues
            std::system(cmd.c_str());
            try {
                std::ifstream fin(tmpIdx);
                if (fin.good()) {
                    nlohmann::json j; fin >> j; fin.close();
                    if (j.is_object() && j.contains("items") && j["items"].is_array()) {
                        for (const auto& it : j["items"]) {
                            if (it.contains("relative_path") && it["relative_path"].is_string()) {
                                ui.gitHubRemotePaths.insert(it["relative_path"].get<std::string>());
                            }
                        }
                        // summarize remote-only sample
                        ui.gitHubRemoteOnlyCount = 0; ui.gitHubRemoteOnlySample.clear();
                        for (const auto &rel : ui.gitHubRemotePaths) {
                            std::string full = ui.exeDir + "/repos/" + ui.selectedRepo + "/" + rel;
                            if (!std::filesystem::exists(full)) {
                                ui.gitHubRemoteOnlyCount++;
                                if (ui.gitHubRemoteOnlySample.size() < 3)
                                    ui.gitHubRemoteOnlySample.push_back(rel);
                            }
                        }
                        ui.gitHubCompareReady = true;
                    }
                }
            } catch(...) {}
            std::filesystem::remove(tmpIdx);
            ui.gitHubCompareInProgress = false;
        }
        // Selection macros (operate on visible rows per current filters)
        {
            const auto& itemsAll = repo.index().items;
            std::vector<int> visible;
            auto contains_icase2 = [](const std::string& hay, const std::string& needle) {
                if (needle.empty()) return true;
                std::string H = hay, N = needle;
                std::transform(H.begin(), H.end(), H.begin(), [](unsigned char c){ return (char)std::tolower(c); });
                std::transform(N.begin(), N.end(), N.begin(), [](unsigned char c){ return (char)std::tolower(c); });
                return H.find(N) != std::string::npos;
            };
            for (int idx = 0; idx < (int)itemsAll.size(); ++idx) {
                const auto& it = itemsAll[idx];
                bool typeOk = (it.type == core::ContentType::PK3 && ui.filterPK3) || (it.type == core::ContentType::CFG && ui.filterCFG) || (it.type == core::ContentType::EXECUTABLE && ui.filterEXE);
                if (!typeOk) continue;
                if (!contains_icase2(it.name, ui.filterName)) continue;
                if (!contains_icase2(it.author, ui.filterAuthor)) continue;
                if (ui.filterSearch[0] != '\0' && !(contains_icase2(it.name, ui.filterSearch) || contains_icase2(it.relativePath, ui.filterSearch))) continue;
                if (ui.filterTag[0] != '\0') {
                    bool any = false;
                    for (const auto& t : it.tags) { if (contains_icase2(t, ui.filterTag)) { any = true; break; } }
                    if (!any) continue;
                }
                visible.push_back(idx);
            }
            if (!visible.empty()) {
                if (ImGui::SmallButton("Select All")) {
                    for (int idx : visible) ui.selectedItemIdsSet.insert(itemsAll[idx].id);
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Deselect All")) {
                    for (int idx : visible) ui.selectedItemIdsSet.erase(itemsAll[idx].id);
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Invert Selection")) {
                    for (int idx : visible) {
                        const std::string& id = itemsAll[idx].id;
                        if (ui.selectedItemIdsSet.count(id)) ui.selectedItemIdsSet.erase(id); else ui.selectedItemIdsSet.insert(id);
                    }
                }
                ImGui::Separator();
            }
        }

        ImGui::BeginChild("items_container", ImVec2(0, 0), false);
        if (ImGui::BeginTable("items", 7, ImGuiTableFlags_RowBg|ImGuiTableFlags_Borders|ImGuiTableFlags_SizingStretchProp|ImGuiTableFlags_Resizable)) {
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Author", ImGuiTableColumnFlags_WidthFixed, 160.0f);
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 60.0f);
            ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn("Updated", ImGuiTableColumnFlags_WidthFixed, 140.0f);
            ImGui::TableSetupColumn("Sync", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableHeadersRow();
            const auto& items = repo.index().items;
            // Build filtered index list
            std::vector<int> order;
            order.reserve(items.size());
            auto contains_icase = [](const std::string& hay, const std::string& needle) {
                if (needle.empty()) return true;
                std::string H = hay, N = needle;
                std::transform(H.begin(), H.end(), H.begin(), [](unsigned char c){ return (char)std::tolower(c); });
                std::transform(N.begin(), N.end(), N.begin(), [](unsigned char c){ return (char)std::tolower(c); });
                return H.find(N) != std::string::npos;
            };
            for (int idx = 0; idx < (int)items.size(); ++idx) {
                const auto& it = items[idx];
                // Type filter
                bool typeOk = (it.type == core::ContentType::PK3 && ui.filterPK3) || (it.type == core::ContentType::CFG && ui.filterCFG) || (it.type == core::ContentType::EXECUTABLE && ui.filterEXE);
                if (!typeOk) continue;
                // Name/author filters
                if (!contains_icase(it.name, ui.filterName)) continue;
                if (!contains_icase(it.author, ui.filterAuthor)) continue;
                // Live search over name or path
                if (ui.filterSearch[0] != '\0' && !(contains_icase(it.name, ui.filterSearch) || contains_icase(it.relativePath, ui.filterSearch))) continue;
                // Tag filter: match substring in any tag if provided
                if (ui.filterTag[0] != '\0') {
                    bool any = false;
                    for (const auto& t : it.tags) {
                        if (contains_icase(t, ui.filterTag)) { any = true; break; }
                    }
                    if (!any) continue;
                }
                order.push_back(idx);
            }
            // Sort
            std::sort(order.begin(), order.end(), [&](int a, int b){
                const auto& A = items[a];
                const auto& B = items[b];
                bool less = false;
                if (ui.sortField == 0) {
                    std::string an = A.name, bn = B.name;
                    std::transform(an.begin(), an.end(), an.begin(), [](unsigned char c){ return (char)std::tolower(c); });
                    std::transform(bn.begin(), bn.end(), bn.begin(), [](unsigned char c){ return (char)std::tolower(c); });
                    less = an < bn;
                } else if (ui.sortField == 1) {
                    less = A.updatedAt < B.updatedAt;
                } else {
                    less = A.fileSizeBytes < B.fileSizeBytes;
                }
                return ui.sortDesc ? !less : less;
            });
            // Render rows
            ImGuiIO& io = ImGui::GetIO();
            if (!ImGui::IsMouseDown(0)) { ui.draggingSelect = false; ui.dragStartVisIndex = -1; }
            for (int vis = 0; vis < (int)order.size(); ++vis) {
                int i = order[vis];
                const auto& it = items[i];
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                bool sel = (ui.selectedItemIdsSet.find(it.id) != ui.selectedItemIdsSet.end());
                if (ImGui::Selectable((it.name + "##" + it.id).c_str(), sel, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick)) {
                    if (io.KeyShift && ui.selectedItemIndex >= 0) {
                        // Range select between anchor (selectedItemIndex) and current (vis) respecting current filter order
                        int anchorVis = -1;
                        for (int k = 0; k < (int)order.size(); ++k) { if (order[k] == ui.selectedItemIndex) { anchorVis = k; break; } }
                        if (anchorVis != -1) {
                            int a = std::min(anchorVis, vis);
                            int b = std::max(anchorVis, vis);
                            if (!io.KeyCtrl) ui.selectedItemIdsSet.clear();
                            for (int r = a; r <= b; ++r) {
                                const auto& rit = items[order[r]];
                                ui.selectedItemIdsSet.insert(rit.id);
                            }
                        } else {
                            // If anchor not visible, just select current row
                            if (!io.KeyCtrl) ui.selectedItemIdsSet.clear();
                            ui.selectedItemIdsSet.insert(it.id);
                        }
                    } else if (io.KeyCtrl) {
                        if (sel) ui.selectedItemIdsSet.erase(it.id); else ui.selectedItemIdsSet.insert(it.id);
                    } else {
                        ui.selectedItemIdsSet.clear();
                        ui.selectedItemIdsSet.insert(it.id);
                    }
                    ui.selectedItemIndex = i;
                    ui.selectedItemId = it.id;
                    std::snprintf(ui.renameBuffer, sizeof(ui.renameBuffer), "%s", it.name.c_str());

                    // Open Edit Metadata on double-click
                    if (ImGui::IsMouseDoubleClicked(0)) {
                        // Populate edit buffers
                        std::snprintf(ui.editName, sizeof(ui.editName), "%s", it.name.c_str());
                        std::snprintf(ui.editAuthor, sizeof(ui.editAuthor), "%s", it.author.c_str());
                        std::snprintf(ui.editDesc, sizeof(ui.editDesc), "%s", it.description.c_str());
                        // Prepare tags list
                        ui.editTagsList.clear();
                        for (const auto& tag : it.tags) {
                            size_t colonPos = tag.find(':');
                            if (colonPos != std::string::npos) {
                                std::string key = tag.substr(0, colonPos);
                                std::string value = tag.substr(colonPos + 1);
                                ui.editTagsList.push_back(std::make_pair(key, value));
                            } else {
                                ui.editTagsList.push_back(std::make_pair(tag, std::string("")));
                            }
                        }
                        ui.newTagKey[0] = 0;
                        ui.newTagValue[0] = 0;
                        ui.showEditMetadata = true;
                    }
                }
                // Drag-to-select across rows while holding mouse: select a contiguous range
                if (ui.draggingSelect && ui.dragStartVisIndex >= 0 && ImGui::IsItemHovered() && ImGui::IsMouseDown(0)) {
                    int a = std::min(ui.dragStartVisIndex, vis);
                    int b = std::max(ui.dragStartVisIndex, vis);
                    if (!ui.draggingSelectAdditive) {
                        // start from only the anchor selection
                        ui.selectedItemIdsSet.clear();
                        const auto& anchor = items[order[ui.dragStartVisIndex]];
                        ui.selectedItemIdsSet.insert(anchor.id);
                    }
                    for (int r = a; r <= b; ++r) {
                        const auto& rit = items[order[r]];
                        ui.selectedItemIdsSet.insert(rit.id);
                    }
                }
                // (marquee selection removed)

                // Context menu for right-click
                if (ImGui::BeginPopupContextItem(("context_" + it.id).c_str())) {
                    ui.contextMenuItemIndex = i;
                    ui.selectedItemIndex = i;
                    ui.selectedItemId = it.id;
                    std::snprintf(ui.renameBuffer, sizeof(ui.renameBuffer), "%s", it.name.c_str());
                    ImGui::Text("%s", it.name.c_str());
                    ImGui::Separator();
                    if (ImGui::MenuItem("Edit Metadata")) {
                        ui.contextMenuItemIndex = i;
                        std::snprintf(ui.editName, sizeof(ui.editName), "%s", it.name.c_str());
                        std::snprintf(ui.editAuthor, sizeof(ui.editAuthor), "%s", it.author.c_str());
                        std::snprintf(ui.editDesc, sizeof(ui.editDesc), "%s", it.description.c_str());
                        std::string tagsJoined;
                        for (size_t ti=0; ti<it.tags.size(); ++ti) {
                            tagsJoined += it.tags[ti];
                            if (ti+1<it.tags.size()) tagsJoined += ",";
                        }
                        std::snprintf(ui.editTags, sizeof(ui.editTags), "%s", tagsJoined.c_str());
                        // Parse tags into key:value pairs for better editing
                        ui.editTagsList.clear();
                        for (const auto& tag : it.tags) {
                            size_t colonPos = tag.find(':');
                            if (colonPos != std::string::npos) {
                                std::string key = tag.substr(0, colonPos);
                                std::string value = tag.substr(colonPos + 1);
                                ui.editTagsList.push_back(std::make_pair(key, value)); // Fixed brace-init-list issue
                            } else {
                                // Handle tags without colon as key with empty value
                                ui.editTagsList.push_back(std::make_pair(tag, std::string(""))); // Fixed brace-init-list issue
                            }
                        }
                        ui.newTagKey[0] = 0;
                        ui.newTagValue[0] = 0;
                        ui.showEditMetadata = true;
                    }
                    if (ImGui::MenuItem("Copy Relative Path")) {
                        ImGui::SetClipboardText(it.relativePath.c_str());
                    }
                    if (ImGui::MenuItem("Copy Absolute Path")) {
                        std::string fullPath = ui.exeDir + "/repos/" + ui.selectedRepo + "/" + it.relativePath;
                        ImGui::SetClipboardText(fullPath.c_str());
                    }
                    if (ImGui::MenuItem("Open Containing Folder")) {
                        std::string fullPath = ui.exeDir + "/repos/" + ui.selectedRepo + "/" + it.relativePath;
                        std::string parentPath = std::filesystem::path(fullPath).parent_path().string();
                        #ifdef _WIN32
                        // Normalize to Windows separators
                        std::string fullWin = fullPath;
                        for (auto &ch : fullWin) if (ch == '/') ch = '\\';
                        std::string cmd = std::string("explorer.exe /select,\"") + fullWin + "\"";
                        #else
                        std::string cmd = std::string("xdg-open \"") + parentPath + "\"";
                        #endif
                        std::system(cmd.c_str());
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Remove", nullptr, false, true)) {
                        ui.confirmRemoveItem = true;
                    }
                    ImGui::EndPopup();
                }
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(it.relativePath.c_str());
                ImGui::TableSetColumnIndex(2);
                ImGui::TextUnformatted(it.author.c_str());
                ImGui::TableSetColumnIndex(3);
                const char* typeName = "file";
                if (it.type == core::ContentType::PK3) { typeName = "pk3"; }
                else if (it.type == core::ContentType::CFG) { typeName = "cfg"; }
                else if (it.type == core::ContentType::EXECUTABLE) { typeName = "exe"; }
                ImGui::Text("%s", typeName);
                ImGui::TableSetColumnIndex(4);
                // Try to get file size
                std::string fullPath = ui.exeDir + "/repos/" + ui.selectedRepo + "/" + it.relativePath;
                if (std::filesystem::exists(fullPath)) {
                    auto size = std::filesystem::file_size(fullPath);
                    if (size < 1024) {
                        ImGui::Text("%zu B", size);
                    } else if (size < 1024 * 1024) {
                        ImGui::Text("%.1f KB", size / 1024.0);
                    } else {
                        ImGui::Text("%.1f MB", size / (1024.0 * 1024.0));
                    }
                } else {
                    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "Missing");
                }
                ImGui::TableSetColumnIndex(5);
                // Last updated time
                char tbuf[64];
                std::time_t tt = static_cast<time_t>(it.updatedAt);
                std::tm* lt = std::localtime(&tt);
                if (lt && std::strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M", lt)) {
                    ImGui::TextUnformatted(tbuf);
                } else {
                    ImGui::TextUnformatted("-");
                }
                // Sync indicator
                ImGui::TableSetColumnIndex(6);
                if (ui.gitHubCompareReady) {
                    bool localExists = std::filesystem::exists(fullPath);
                    bool remoteHas = (ui.gitHubRemotePaths.find(it.relativePath) != ui.gitHubRemotePaths.end());
                    if (localExists && remoteHas) {
                        ImGui::TextColored(ImVec4(0.6f,1.0f,0.6f,1.0f), "OK");
                    } else if (localExists && !remoteHas) {
                        ImGui::TextColored(ImVec4(1.0f,0.8f,0.4f,1.0f), "Local only");
                    } else if (!localExists && remoteHas) {
                        ImGui::TextColored(ImVec4(1.0f,0.6f,0.6f,1.0f), "Remote only");
                    } else {
                        ImGui::Text("-");
                    }
                } else {
                    ImGui::TextDisabled("-");
                }
            }
            // (marquee rectangle removed)
            ImGui::EndTable();
        }
        ImGui::EndChild();
        // Batch delete popup
        if (ImGui::BeginPopupModal("batch_delete_popup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Delete %d selected items?", (int)ui.selectedItemIdsSet.size());
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "This cannot be undone.");
            if (ImGui::Button("Yes, delete")) {
                for (const auto& id : ui.selectedItemIdsSet) {
                    repo.removeItem(id);
                }
                repo.loadIndex();
                ui.selectedItemIdsSet.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
        // Batch move modal
        if (ui.showBatchMoveModal) ImGui::OpenPopup("batch_move_popup");
        if (ImGui::BeginPopupModal("batch_move_popup", &ui.showBatchMoveModal, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Move %d items to folder:", (int)ui.selectedItemIdsSet.size());
            ImGui::InputText("Target folder", ui.batchMoveTarget, IM_ARRAYSIZE(ui.batchMoveTarget));
            if (ImGui::Button("Move")) {
                std::string folder = ui.batchMoveTarget;
                if (!folder.empty() && folder.back() != '/') folder += '/';
                core::RepoManager r2(repoRoot);
                r2.loadIndex();
                for (const auto& id : ui.selectedItemIdsSet) {
                    // Find current item to keep filename
                    auto& items2 = r2.index();
                    auto it2 = std::find_if(items2.items.begin(), items2.items.end(), [&](const core::ContentItem& c){ return c.id == id; });
                    if (it2 != items2.items.end()) {
                        std::string filename;
                        auto pos = it2->relativePath.find_last_of('/');
                        filename = (pos == std::string::npos) ? it2->relativePath : it2->relativePath.substr(pos+1);
                        r2.moveItem(id, folder + filename);
                    }
                }
                r2.loadIndex();
                repo.loadIndex();
                ui.selectedItemIdsSet.clear();
                ui.batchMoveTarget[0] = 0;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) { ui.showBatchMoveModal = false; ImGui::CloseCurrentPopup(); }
            ImGui::EndPopup();
        }
        // Batch tags modal
        if (ui.showBatchTagsModal) ImGui::OpenPopup("batch_tags_popup");
        if (ImGui::BeginPopupModal("batch_tags_popup", &ui.showBatchTagsModal, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Batch operations for %d items", (int)ui.selectedItemIdsSet.size());
            ImGui::Separator();
            // Author update
            ImGui::Checkbox("Set Author", &ui.batchAuthorSet);
            ImGui::SameLine();
            ImGui::BeginDisabled(!ui.batchAuthorSet);
            ImGui::InputText("##batch_author", ui.batchAuthor, IM_ARRAYSIZE(ui.batchAuthor));
            ImGui::EndDisabled();
            ImGui::Separator();
            ImGui::Text("Add tag (key:value)");
            ImGui::InputText("Key", ui.batchAddTagKey, IM_ARRAYSIZE(ui.batchAddTagKey));
            ImGui::SameLine();
            ImGui::InputText("Value", ui.batchAddTagValue, IM_ARRAYSIZE(ui.batchAddTagValue));
            ImGui::Text("Add multiple (comma-separated key[:value])");
            ImGui::InputText("Multi", ui.batchAddMulti, IM_ARRAYSIZE(ui.batchAddMulti));
            ImGui::Text("Remove tag by key");
            ImGui::InputText("Key##remove", ui.batchRemoveTagKey, IM_ARRAYSIZE(ui.batchRemoveTagKey));
            ImGui::Checkbox("Keep selection after apply", &ui.batchKeepSelection);
            if (ImGui::Button("Apply")) {
                std::string addKey = ui.batchAddTagKey;
                std::string addVal = ui.batchAddTagValue;
                std::string removeKey = ui.batchRemoveTagKey;
                std::string addMulti = ui.batchAddMulti;
                core::RepoManager r2(repoRoot);
                r2.loadIndex();
                for (const auto& id : ui.selectedItemIdsSet) {
                    // Read current item
                    const auto& items2 = r2.index().items;
                    auto it2 = std::find_if(items2.begin(), items2.end(), [&](const core::ContentItem& c){ return c.id == id; });
                    if (it2 == items2.end()) continue;
                    std::vector<std::string> newTags = it2->tags;
                    std::string newAuthor = it2->author;
                    if (ui.batchAuthorSet) newAuthor = ui.batchAuthor;
                    // Remove by key
                    if (!removeKey.empty()) {
                        newTags.erase(std::remove_if(newTags.begin(), newTags.end(), [&](const std::string& t){
                            auto cp = t.find(':');
                            std::string k = (cp==std::string::npos)? t : t.substr(0, cp);
                            return k == removeKey;
                        }), newTags.end());
                    }
                    // Add key:value if provided and not duplicate key
                    if (!addKey.empty()) {
                        bool exists = false;
                        for (const auto& t : newTags) {
                            auto cp = t.find(':');
                            std::string k = (cp==std::string::npos)? t : t.substr(0, cp);
                            if (k == addKey) { exists = true; break; }
                        }
                        if (!exists) {
                            newTags.push_back(addVal.empty() ? addKey : (addKey + ":" + addVal));
                        }
                    }
                    // Add multiple
                    if (!addMulti.empty()) {
                        size_t pos = 0;
                        while (pos < addMulti.size()) {
                            size_t comma = addMulti.find(',', pos);
                            std::string token = addMulti.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
                            // trim
                            auto l = token.find_first_not_of(" \t");
                            auto r = token.find_last_of(" \t");
                            if (l != std::string::npos) token = token.substr(l, r==std::string::npos?std::string::npos:(r-l+1)); else token.clear();
                            if (!token.empty()) {
                                std::string key = token; std::string value;
                                auto cp = token.find(':');
                                if (cp != std::string::npos) { key = token.substr(0, cp); value = token.substr(cp+1); }
                                bool exists = false;
                                for (const auto& t : newTags) {
                                    auto c2 = t.find(':');
                                    std::string k2 = (c2==std::string::npos)? t : t.substr(0, c2);
                                    if (k2 == key) { exists = true; break; }
                                }
                                if (!exists) newTags.push_back(value.empty()? key : (key+":"+value));
                            }
                            if (comma == std::string::npos) break; else pos = comma + 1;
                        }
                    }
                    r2.updateItemMetadata(id, it2->name, it2->description, newAuthor, newTags);
                }
                r2.loadIndex();
                repo.loadIndex();
                if (!ui.batchKeepSelection) ui.selectedItemIdsSet.clear();
                ui.batchAddTagKey[0] = ui.batchAddTagValue[0] = ui.batchRemoveTagKey[0] = ui.batchAddMulti[0] = 0;
                ui.batchAuthor[0] = 0; ui.batchAuthorSet = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) { ui.showBatchTagsModal = false; ImGui::CloseCurrentPopup(); }
            ImGui::EndPopup();
        }
        // Stats panel
        ImGui::Separator();
        uint64_t totalSize = 0;
        size_t cntPK3=0, cntCFG=0, cntEXE=0;
        for (const auto& it : repo.index().items) {
            totalSize += it.fileSizeBytes;
            if (it.type == core::ContentType::PK3) ++cntPK3;
            else if (it.type == core::ContentType::CFG) ++cntCFG;
            else if (it.type == core::ContentType::EXECUTABLE) ++cntEXE;
        }
        ImGui::Text("Total size: %.2f MB", totalSize / (1024.0 * 1024.0));
        ImGui::SameLine(); ImGui::Text("| pk3: %zu", cntPK3);
        ImGui::SameLine(); ImGui::Text("| cfg: %zu", cntCFG);
        ImGui::SameLine(); ImGui::Text("| exe: %zu", cntEXE);
        // Edit metadata popup - check flag and open
        if (ui.showEditMetadata) {
            ImGui::OpenPopup("edit_metadata_popup");
            ui.showEditMetadata = false;
        }
        if (ImGui::BeginPopupModal("edit_metadata_popup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Edit item metadata:");
            ImGui::Separator();
            ImGui::InputText("Name", ui.editName, IM_ARRAYSIZE(ui.editName));
            ImGui::InputText("Author", ui.editAuthor, IM_ARRAYSIZE(ui.editAuthor));
            ImGui::InputTextMultiline("Description", ui.editDesc, IM_ARRAYSIZE(ui.editDesc), ImVec2(480, 120));
            // Tags management with key:value pairs
            ImGui::Text("Tags (key:value format):");
            ImGui::BeginChild("tags_list", ImVec2(480, 140), true);
            for (size_t ti = 0; ti < ui.editTagsList.size(); ++ti) {
                ImGui::PushID((int)ti);
                auto& tagPair = ui.editTagsList[ti];
                // Editable key and value fields
                char keyBuf[64], valueBuf[64];
                std::snprintf(keyBuf, sizeof(keyBuf), "%s", tagPair.first.c_str());
                std::snprintf(valueBuf, sizeof(valueBuf), "%s", tagPair.second.c_str());
                ImGui::SetNextItemWidth(80);
                if (ImGui::InputText("##key", keyBuf, sizeof(keyBuf))) {
                    tagPair.first = keyBuf;
                }
                ImGui::SameLine();
                ImGui::Text(":");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(80);
                if (ImGui::InputText("##value", valueBuf, sizeof(valueBuf))) {
                    tagPair.second = valueBuf;
                }
                ImGui::SameLine();
                // Move up/down buttons
                if (ti > 0 && ImGui::SmallButton("^")) {
                    std::swap(ui.editTagsList[ti], ui.editTagsList[ti-1]);
                }
                ImGui::SameLine();
                if (ti < ui.editTagsList.size()-1 && ImGui::SmallButton("v")) {
                    std::swap(ui.editTagsList[ti], ui.editTagsList[ti+1]);
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Remove")) {
                    ui.editTagsList.erase(ui.editTagsList.begin() + ti);
                    --ti; // Adjust index after removal
                }
                ImGui::PopID();
            }
            ImGui::EndChild();
            // Add new tag with key:value format
            ImGui::Text("Add new tag:");
            ImGui::InputText("Key", ui.newTagKey, IM_ARRAYSIZE(ui.newTagKey));
            ImGui::SameLine();
            ImGui::InputText("Value", ui.newTagValue, IM_ARRAYSIZE(ui.newTagValue));
            ImGui::SameLine();
            if (ImGui::Button("Add Tag") && strlen(ui.newTagKey) > 0) {
                std::string key = ui.newTagKey;
                std::string value = ui.newTagValue;
                // Check if key already exists
                bool exists = false;
                for (const auto& tagPair : ui.editTagsList) {
                    if (tagPair.first == key) {
                        exists = true;
                        break;
                    }
                }
                if (!exists) {
                    ui.editTagsList.push_back(std::make_pair(key, value)); // Fixed brace-init-list issue
                }
                ui.newTagKey[0] = 0;
                ui.newTagValue[0] = 0;
            }
            ImGui::Separator();
            if (ImGui::Button("Apply")) {
                if (ui.contextMenuItemIndex >= 0) {
                    // Convert key:value pairs back to string format
                    std::vector<std::string> tags;
                    for (const auto& tagPair : ui.editTagsList) {
                        if (tagPair.second.empty()) {
                            tags.push_back(tagPair.first);
                        } else {
                            tags.push_back(tagPair.first + ":" + tagPair.second);
                        }
                    }
                    core::RepoManager r2(repoRoot); // repoRoot now in scope
                    r2.loadIndex();
                    if (ui.contextMenuItemIndex >= 0 && ui.contextMenuItemIndex < (int)r2.index().items.size()) {
                        const auto& item2 = r2.index().items[ui.contextMenuItemIndex];
                        r2.updateItemMetadata(item2.id, ui.editName, ui.editDesc, ui.editAuthor, tags);
                        r2.loadIndex();
                        repo.loadIndex(); // Reload original repo manager
                    }
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) { ImGui::CloseCurrentPopup(); }
            ImGui::EndPopup();
        }
        // Remove confirmation popup
        if (ui.confirmRemoveItem) {
            ImGui::OpenPopup("remove_item_popup");
        }
        if (ImGui::BeginPopupModal("remove_item_popup", &ui.confirmRemoveItem, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Remove selected item from repository?");
            ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "This will delete the file permanently!");
            ImGui::Separator();
            if (ui.selectedItemIndex >= 0 && ui.selectedItemIndex < (int)repo.index().items.size()) {
                const auto& item = repo.index().items[ui.selectedItemIndex];
                ImGui::Text("Name: %s", item.name.c_str());
                ImGui::Text("Path: %s", item.relativePath.c_str());
            }
            ImGui::Separator();
            if (ImGui::Button("Yes, Remove")) {
                repo.removeItem(ui.selectedItemId);
                repo.loadIndex();
                ui.selectedItemIndex = -1;
                ui.selectedItemId.clear();
                ui.confirmRemoveItem = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                ui.confirmRemoveItem = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    } else {
        ImGui::TextUnformatted("Select or init a repository to manage items.");
    }
    // ==================
    // GitHub integration
    // ==================
    ImGui::Separator();
    std::string currentUser = config::Config::getInstance().getGithubUser();
    if (currentUser.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.6f, 1.0f), "Not logged in");
    } else {
        ImGui::TextColored(ImVec4(0.6f, 1.0f, 0.6f, 1.0f), "Logged in as: %s", currentUser.c_str());
    }
    // GitHub window is handled separately in main_gui.cpp
    ImGui::End();
}
} } // namespace ui::menus