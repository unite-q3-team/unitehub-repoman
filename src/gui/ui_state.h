#ifndef GUI_UI_STATE_H
#define GUI_UI_STATE_H

#include <string>
#include <vector>
#include <unordered_set>

namespace ui {

enum class Operation {
    None,
    Clone,
    Pull, 
    Push,
    ListRepos,
    DeleteRepo,
    UpdateVisibility
};

struct GitHubRepoInfo {
    std::string name;
    std::string full_name;
    std::string description;
    bool is_private;
    std::string html_url;
    std::string default_branch = "main";
    bool is_compatible = false;
    bool compatibility_checked = false;
};

struct State {
    std::string exeDir;
    std::string selectedRepo;
    std::vector<std::string> repoNames;

    bool showAddModal = false;
    char addSrc[1024] = {0};
    int addType = 0; // 0=pk3,1=cfg,2=exe
    char addRel[1024] = {0};
    char addName[256] = {0};
    char addAuthor[256] = {0};
    char addDesc[512] = {0};
    char addTags[512] = {0}; // comma-separated (legacy)
    // New tag system for add item
    std::vector<std::pair<std::string, std::string>> addTagsList; // key, value pairs
    char addNewTagKey[64] = {0};
    char addNewTagValue[64] = {0};

    int selectedItemIndex = -1;
    std::string selectedItemId;
    char renameBuffer[256] = {0};
    bool confirmDeleteRepo = false;
    bool confirmRemoveItem = false;
    bool showEditMetadata = false;
    char editName[256] = {0};
    char editAuthor[256] = {0};
    char editDesc[512] = {0};
    char editTags[512] = {0};
    // Tag management (key:value format)
    std::vector<std::pair<std::string, std::string>> editTagsList; // key, value pairs
    char newTagKey[64] = {0};
    char newTagValue[64] = {0};
    
    // Repository management
    bool showRenameRepoModal = false;
    char renameRepoBuffer[128] = {0};

    // Drag-and-drop queue of absolute file paths
    std::vector<std::string> dropQueue;
    std::vector<bool> dropInclude; // which dropped files are included

    // Filtering / search / sorting
    char filterSearch[128] = {0}; // live search by name/path
    char filterName[128] = {0};
    char filterAuthor[128] = {0};
    char filterTag[128] = {0}; // key or key:value
    bool filterPK3 = true;
    bool filterCFG = true;
    bool filterEXE = true;
    int sortField = 0; // 0 = Name, 1 = Updated, 2 = Size
    bool sortDesc = false;
    bool showFiltersWindow = false;
    // Requests triggered from top menu
    bool requestRescan = false;

    // Multi-selection for batch operations
    std::unordered_set<std::string> selectedItemIdsSet;
    bool draggingSelect = false;
    bool draggingSelectAdditive = false;
    int dragStartVisIndex = -1;
    // (marquee selection removed)
    bool showBatchMoveModal = false;
    bool showBatchTagsModal = false;
    char batchMoveTarget[256] = {0}; // target folder (relative)
    // batch tags ops
    char batchAddTagKey[64] = {0};
    char batchAddTagValue[64] = {0};
    char batchRemoveTagKey[64] = {0};
    // batch author update
    bool batchAuthorSet = false;
    char batchAuthor[256] = {0};
    bool batchKeepSelection = true; // do not clear selection after apply
    char batchAddMulti[512] = {0}; // comma-separated key[:value]

    // GitHub interface improvements
    char gitHubRemote[256] = {0}; // unified remote field
    char gitHubBranch[64] = "main"; // unified branch field
    char gitHubLocalName[128] = {0}; // for clone operations
    bool gitHubSetCurrent = false;
    bool gitHubForce = false;
    bool gitHubCreate = false;
    bool gitHubPrivate = false;
    int gitHubVisibility = 0; // 0=public, 1=private
    
    // Progress tracking
    Operation currentOperation = Operation::None;
    bool operationInProgress = false;
    std::string operationStatus;
    float operationProgress = 0.0f;
    
    // GitHub repos list
    std::vector<GitHubRepoInfo> gitHubRepos;
    std::string githubOutput; // for messages and logs
    
    // Context menu
    bool showContextMenu = false;
    int contextMenuItemIndex = -1;
    
    // GitHub window
    bool showGitHubWindow = false;
    // Help/About windows
    bool showHelpWindow = false;
    bool showAboutWindow = false;
    // GitHub repo context actions
    bool showGithubRenamePopup = false;
    char githubRenameBuffer[128] = {0};
    char gitHubSelectedFullName[256] = {0};
    // Remote index compare
    bool gitHubCompareReady = false;
    std::unordered_set<std::string> gitHubRemotePaths;
    int gitHubRemoteOnlyCount = 0;
    std::vector<std::string> gitHubRemoteOnlySample;
    bool gitHubCompareInProgress = false;
};

void refreshRepos(State& s);
void resetGitHubOperation(State& s);
void parseGitHubReposList(const std::string& json, std::vector<GitHubRepoInfo>& repos);
void checkRepoCompatibility(const std::string& exeDir, const std::string& token, GitHubRepoInfo& repo);

}

#endif


