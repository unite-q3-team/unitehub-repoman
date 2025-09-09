// Single-file GUI entry that wires platform loop and calls menu renderers
#include <cstdio>
#include <string>
#include <vector>
#include <filesystem>
#include <unordered_map>
#include <cstring>

#include "system/fs.h"
#include "system/config.h"
#include "system/logger.h"
#include "core/repo.h"
#include "core/types.h"
#include "utils/hash.h"

// Dear ImGui
#include "imgui.h"
#include "gui/ui_state.h"
static ui::State* g_ui_state = nullptr;
#ifdef _WIN32
#include <windows.h>
#include <GL/gl.h>
#include <shellapi.h>
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_opengl2.h"
// Forward declaration (C++ linkage)
LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    #ifdef _WIN32
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return 1;
    if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
    if (msg == WM_DROPFILES) {
        HDROP hDrop = (HDROP)wParam;
        UINT num = DragQueryFileA(hDrop, 0xFFFFFFFF, NULL, 0);
        char path[MAX_PATH];
        for (UINT i = 0; i < num; ++i) {
            UINT len = DragQueryFileA(hDrop, i, path, MAX_PATH);
            if (len > 0 && g_ui_state) { g_ui_state->dropQueue.emplace_back(path); }
        }
        DragFinish(hDrop);
        if (g_ui_state) g_ui_state->showAddModal = true;
        return 0;
    }
    #endif
    return DefWindowProc(hWnd, msg, wParam, lParam);
}
#else
#include <GLFW/glfw3.h>
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl2.h"
static ui::State* g_ui_state_glfw = nullptr;
static void GlfwDropCallback(GLFWwindow*, int count, const char** paths)
{
    if (!g_ui_state_glfw) return;
    for (int i = 0; i < count; ++i) g_ui_state_glfw->dropQueue.emplace_back(paths[i]);
    g_ui_state_glfw->showAddModal = true;
}
#endif

// Forward declarations of modular UI
#include "gui/ui_state.h"

void ui::refreshRepos(ui::State& s)
{
    s.repoNames.clear();
    std::string reposDir = s.exeDir + "/repos";
    if (std::filesystem::exists(reposDir)) {
        for (auto& e : std::filesystem::directory_iterator(reposDir)) {
            if (e.is_directory()) s.repoNames.push_back(e.path().filename().string());
        }
    }
}

void ui::resetGitHubOperation(ui::State& s)
{
    s.currentOperation = ui::Operation::None;
    s.operationInProgress = false;
    s.operationProgress = 0.0f;
    s.operationStatus.clear();
}

void ui::parseGitHubReposList(const std::string& json, std::vector<ui::GitHubRepoInfo>& repos)
{
    repos.clear();
    // Simple JSON parsing for GitHub repos
    size_t pos = 0;
    while ((pos = json.find("\"name\":", pos)) != std::string::npos) {
        ui::GitHubRepoInfo repo;
        
        // Parse name
        auto nameStart = json.find("\"", pos + 7);
        if (nameStart != std::string::npos) {
            auto nameEnd = json.find("\"", nameStart + 1);
            if (nameEnd != std::string::npos) {
                repo.name = json.substr(nameStart + 1, nameEnd - nameStart - 1);
            }
        }
        
        // Parse full_name
        auto fullStart = json.find("\"full_name\":", pos);
        if (fullStart != std::string::npos && fullStart < json.find("\"name\":", pos + 10)) {
            fullStart = json.find("\"", fullStart + 12);
            if (fullStart != std::string::npos) {
                auto fullEnd = json.find("\"", fullStart + 1);
                if (fullEnd != std::string::npos) {
                    repo.full_name = json.substr(fullStart + 1, fullEnd - fullStart - 1);
                }
            }
        }
        
        // Parse description
        auto descStart = json.find("\"description\":", pos);
        if (descStart != std::string::npos && descStart < json.find("\"name\":", pos + 10)) {
            descStart = json.find("\"", descStart + 14);
            if (descStart != std::string::npos) {
                auto descEnd = json.find("\"", descStart + 1);
                if (descEnd != std::string::npos) {
                    repo.description = json.substr(descStart + 1, descEnd - descStart - 1);
                }
            }
        }
        
        // Parse private status
        auto privStart = json.find("\"private\":", pos);
        if (privStart != std::string::npos && privStart < json.find("\"name\":", pos + 10)) {
            auto boolStart = json.find_first_of("tf", privStart + 10);
            if (boolStart != std::string::npos) {
                repo.is_private = (json.substr(boolStart, 4) == "true");
            }
        }
        
        // Parse html_url
        auto urlStart = json.find("\"html_url\":", pos);
        if (urlStart != std::string::npos && urlStart < json.find("\"name\":", pos + 10)) {
            urlStart = json.find("\"", urlStart + 11);
            if (urlStart != std::string::npos) {
                auto urlEnd = json.find("\"", urlStart + 1);
                if (urlEnd != std::string::npos) {
                    repo.html_url = json.substr(urlStart + 1, urlEnd - urlStart - 1);
                }
            }
        }
        
        // Parse default_branch
        auto branchStart = json.find("\"default_branch\":", pos);
        if (branchStart != std::string::npos && branchStart < json.find("\"name\":", pos + 10)) {
            branchStart = json.find("\"", branchStart + 17);
            if (branchStart != std::string::npos) {
                auto branchEnd = json.find("\"", branchStart + 1);
                if (branchEnd != std::string::npos) {
                    repo.default_branch = json.substr(branchStart + 1, branchEnd - branchStart - 1);
                }
            }
        }
        
        if (!repo.name.empty()) {
            repos.push_back(repo);
        }
        
        pos = json.find("\"name\":", pos + 10);
    }
}

void ui::checkRepoCompatibility(const std::string& exeDir, const std::string& token, ui::GitHubRepoInfo& repo)
{
    if (repo.compatibility_checked) return;
    
    // Check compatibility by trying to fetch index.json from default branch
    std::string tmpIdx = exeDir + "/gh_idx_check.json";
    std::string rawUrl = std::string("https://raw.githubusercontent.com/") + repo.full_name + "/" + repo.default_branch + "/index.json";
    std::string getIdx = std::string("curl -sL -H \"Authorization: token ") + token + "\" -H \"Accept: application/vnd.github.v3.raw\" \"" + rawUrl + "\" -o \"" + tmpIdx + "\"";
    int rc = std::system(getIdx.c_str()); (void)rc;
    
    bool compatible = false;
    try {
        std::ifstream fi(tmpIdx);
        if (fi.good()) {
            // Simple JSON validation - check if it contains required fields
            std::string content((std::istreambuf_iterator<char>(fi)), std::istreambuf_iterator<char>());
            fi.close();
            
            // Basic check for required JSON structure
            compatible = (content.find("\"version\"") != std::string::npos) && 
                        (content.find("\"items\"") != std::string::npos) &&
                        (content.find("[") != std::string::npos); // items should be an array
        }
    } catch (...) { 
        /* ignore parsing errors */ 
    }
    std::filesystem::remove(tmpIdx);
    
    repo.is_compatible = compatible;
    repo.compatibility_checked = true;
}

// Use separated theme
#include "gui/themes/red.h"

// Menu renderer
#include "gui/menus/main_window.h"
// New modular windows
#include "gui/menus/filters_window.h"
#include "gui/menus/about_window.h"
#include "gui/menus/help_window.h"
// Embedded fonts
#include "fonts/unispace_rg.h"
// Embedded assets
#include "assets/logo_png.h"

// stb_image for decoding PNG from memory
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

static int RunGui()
{
    logger::setLevel(logger::Level::INFO);
    std::string exe = fs::getExecutablePath();
    logger::setLogFile(exe + "/repoman.log");

    ui::State ui{}; g_ui_state = &ui;
    ui.exeDir = exe;
    config::loadConfig(exe + "/config.json");
    ui.selectedRepo = config::getCurrentRepo();
    ui::refreshRepos(ui);

#ifdef _WIN32
    // Win32 + OpenGL2 backend
    WNDCLASSEXA wc = { sizeof(WNDCLASSEXA), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandleA(NULL), NULL, NULL, NULL, NULL, "RepoMan", NULL };
    RegisterClassExA(&wc);
    HWND hwnd = CreateWindowA(wc.lpszClassName, "RepoMan", WS_OVERLAPPEDWINDOW, 100, 100, 1000, 700, NULL, NULL, wc.hInstance, NULL);
    HDC hdc = GetDC(hwnd);

    // Setup pixel format and OpenGL context
    PIXELFORMATDESCRIPTOR pfd = { sizeof(PIXELFORMATDESCRIPTOR), 1,
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,  // Flags
        PFD_TYPE_RGBA,        // Pixel type
        32,                   // Color depth
        0, 0, 0, 0, 0, 0,
        0,                    // Alpha
        0,                    // Shift bit
        0,                    // Accum
        0, 0, 0, 0,
        24,                   // Depth
        8,                    // Stencil
        0,                    // Aux
        PFD_MAIN_PLANE,
        0, 0, 0, 0 };
    int pf = ChoosePixelFormat(hdc, &pfd);
    SetPixelFormat(hdc, pf, &pfd);
    HGLRC hrc = wglCreateContext(hdc);
    wglMakeCurrent(hdc, hrc);
    // Dear ImGui init
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    { ImGuiIO& io = ImGui::GetIO(); io.IniFilename = nullptr; }
    ImGui::StyleColorsDark();
    ui::themes::applyRed();
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplOpenGL2_Init();
    // Load bundled font (Unispace Regular) as default
    {
        ImGuiIO& io = ImGui::GetIO();
        ImFontConfig cfg; cfg.FontDataOwnedByAtlas = false;
        io.Fonts->AddFontFromMemoryTTF((void*)unispace_rg_otf, (int)unispace_rg_otf_len, 13.0f, &cfg);
        io.FontDefault = io.Fonts->Fonts.back();
    }
    DragAcceptFiles(hwnd, TRUE);
    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);
    bool running = true;
    bool showMain = true;
    MSG msg;
    // Logo texture persistent across frames to allow proper cleanup
    GLuint gLogoTex = 0; int gLogoW = 0, gLogoH = 0;
    while (running) {
        while (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
            if (msg.message == WM_QUIT) running = false;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        ImGui_ImplOpenGL2_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        // Global shortcuts (Windows path)
        {
            ImGuiIO& io = ImGui::GetIO();
            bool ctrl = io.KeyCtrl;
            bool shift = io.KeyShift;
            auto pressed = [](ImGuiKey key){ return ImGui::IsKeyPressed(key, false); };
            if (!io.WantTextInput) {
                if (ctrl && pressed(ImGuiKey_N)) ui.showAddModal = true;               // Ctrl+N Add item
                if (pressed(ImGuiKey_F5)) ui.requestRescan = true;                    // F5 Rescan
                if (ctrl && shift && pressed(ImGuiKey_R)) ui::refreshRepos(ui);       // Ctrl+Shift+R Refresh repos
                if (ctrl && pressed(ImGuiKey_F)) ui.showFiltersWindow = true;         // Ctrl+F Filters
                if (ctrl && pressed(ImGuiKey_G)) ui.showGitHubWindow = true;          // Ctrl+G GitHub Manager
                if (pressed(ImGuiKey_F1)) ui.showHelpWindow = true;                   // F1 Help
                if (ctrl && pressed(ImGuiKey_E) && !ui.selectedRepo.empty()) {        // Ctrl+E Rename repo
                    std::snprintf(ui.renameRepoBuffer, sizeof(ui.renameRepoBuffer), "%s", ui.selectedRepo.c_str());
                    ui.showRenameRepoModal = true;
                }
                if (ctrl && shift && pressed(ImGuiKey_N)) ImGui::OpenPopup("init_repo"); // Ctrl+Shift+N New repo
                if (ctrl && pressed(ImGuiKey_Delete) && !ui.selectedRepo.empty()) ui.confirmDeleteRepo = true; // Ctrl+Del Delete repo
            }
        }
        // Ensure logo texture and draw centered in viewport background (25% scale)
        if (gLogoTex == 0) {
            int w=0,h=0,n=0;
            unsigned char* rgba = stbi_load_from_memory((const unsigned char*)logo_png_otf, (int)logo_png_otf_len, &w, &h, &n, 4);
            if (rgba) {
                gLogoW = w; gLogoH = h;
                glGenTextures(1, &gLogoTex);
                glBindTexture(GL_TEXTURE_2D, gLogoTex);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
                glBindTexture(GL_TEXTURE_2D, 0);
                stbi_image_free(rgba);
            }
        }
        if (gLogoTex != 0) {
            ImGuiViewport* vp = ImGui::GetMainViewport();
            ImVec2 center(vp->Pos.x + vp->Size.x * 0.5f, vp->Pos.y + vp->Size.y * 0.5f);
            float scale = 0.25f;
            ImVec2 sz((float)gLogoW * scale, (float)gLogoH * scale);
            ImVec2 pmin(center.x - sz.x * 0.5f, center.y - sz.y * 0.5f);
            ImVec2 pmax(center.x + sz.x * 0.5f, center.y + sz.y * 0.5f);
            ImGui::GetBackgroundDrawList()->AddImage((ImTextureID)(intptr_t)gLogoTex, pmin, pmax);
        }
        // Global top menu bar (viewport)
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("Repository")) {
                if (ImGui::MenuItem("Refresh repos", "Ctrl+Shift+R")) ui::refreshRepos(ui);
                if (ImGui::MenuItem("New repo", "Ctrl+Shift+N")) ImGui::OpenPopup("init_repo");
                if (ImGui::MenuItem("Rename repo", "Ctrl+E", false, !ui.selectedRepo.empty())) {
                    std::snprintf(ui.renameRepoBuffer, sizeof(ui.renameRepoBuffer), "%s", ui.selectedRepo.c_str());
                    ui.showRenameRepoModal = true;
                }
                if (ImGui::MenuItem("Delete repo", "Ctrl+Del", false, !ui.selectedRepo.empty())) ui.confirmDeleteRepo = true;
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Items")) {
                if (ImGui::MenuItem("Add item", "Ctrl+N", false, !ui.selectedRepo.empty())) ui.showAddModal = true;
                if (ImGui::MenuItem("Rescan", "F5", false, !ui.selectedRepo.empty())) ui.requestRescan = true;
                if (ImGui::MenuItem("Verify", nullptr, false, !ui.selectedRepo.empty())) ImGui::OpenPopup("verify_popup");
                if (ImGui::MenuItem("Filters & Sorting", "Ctrl+F", ui.showFiltersWindow, !ui.selectedRepo.empty())) ui.showFiltersWindow = true;
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("GitHub")) {
                if (ImGui::MenuItem("Open GitHub Manager", "Ctrl+G")) ui.showGitHubWindow = true;
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Help")) {
                if (ImGui::MenuItem("About")) ui.showAboutWindow = true;
                if (ImGui::MenuItem("Help", "F1")) ui.showHelpWindow = true;
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }
        ui::menus::drawMain(ui, &showMain);
        ui::menus::drawGitHub(ui);
        ui::menus::drawFiltersWindow(ui);
        ui::menus::drawAboutWindow(ui);
        ui::menus::drawHelpWindow(ui);
        if (!showMain) running = false;
        ImGui::Render();
        RECT rc; GetClientRect(hwnd, &rc);
        glViewport(0, 0, rc.right - rc.left, rc.bottom - rc.top);
        glClearColor(0.10f, 0.10f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
        SwapBuffers(hdc);
    }
    if (gLogoTex != 0) { glDeleteTextures(1, &gLogoTex); gLogoTex = 0; }
    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(hrc);
    DestroyWindow(hwnd);
    UnregisterClassA(wc.lpszClassName, wc.hInstance);
#else
    // GLFW + OpenGL2 backend (broad compatibility)
    if (!glfwInit()) return 1;
    bool isWSL = (std::getenv("WSL_DISTRO_NAME") != nullptr) || (std::getenv("WSLENV") != nullptr);
    glfwWindowHint(GLFW_CONTEXT_RELEASE_BEHAVIOR, GLFW_RELEASE_BEHAVIOR_FLUSH);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    GLFWwindow* window = glfwCreateWindow(1000, 700, "RepoMan", nullptr, nullptr);
    if (!window) { glfwTerminate(); return 1; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(isWSL ? 0 : 1);
    g_ui_state_glfw = &ui;
    glfwSetDropCallback(window, GlfwDropCallback);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    { ImGuiIO& io = ImGui::GetIO(); io.IniFilename = nullptr; }
    ImGui::StyleColorsDark();
    ui::themes::applyRed();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL2_Init();
    // Load bundled font (Unispace Regular) as default
    {
        ImGuiIO& io = ImGui::GetIO();
        ImFontConfig cfg; cfg.FontDataOwnedByAtlas = false;
        io.Fonts->AddFontFromMemoryTTF((void*)unispace_rg_otf, (int)unispace_rg_otf_len, 17.0f, &cfg);
        io.FontDefault = io.Fonts->Fonts.back();
    }

    // Logo texture persistent across frames to allow proper cleanup
    GLuint gLogoTex = 0; int gLogoW = 0, gLogoH = 0;
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        ImGui_ImplOpenGL2_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        // Global shortcuts (GLFW path)
        {
            ImGuiIO& io = ImGui::GetIO();
            bool ctrl = io.KeyCtrl;
            bool shift = io.KeyShift;
            auto pressed = [](ImGuiKey key){ return ImGui::IsKeyPressed(key, false); };
            if (!io.WantTextInput) {
                if (ctrl && pressed(ImGuiKey_N)) ui.showAddModal = true;
                if (pressed(ImGuiKey_F5)) ui.requestRescan = true;
                if (ctrl && shift && pressed(ImGuiKey_R)) ui::refreshRepos(ui);
                if (ctrl && pressed(ImGuiKey_F)) ui.showFiltersWindow = true;
                if (ctrl && pressed(ImGuiKey_G)) ui.showGitHubWindow = true;
                if (pressed(ImGuiKey_F1)) ui.showHelpWindow = true;
                if (ctrl && pressed(ImGuiKey_E) && !ui.selectedRepo.empty()) {
                    std::snprintf(ui.renameRepoBuffer, sizeof(ui.renameRepoBuffer), "%s", ui.selectedRepo.c_str());
                    ui.showRenameRepoModal = true;
                }
                if (ctrl && shift && pressed(ImGuiKey_N)) ImGui::OpenPopup("init_repo");
                if (ctrl && pressed(ImGuiKey_Delete) && !ui.selectedRepo.empty()) ui.confirmDeleteRepo = true;
            }
        }
        // Ensure logo texture and draw centered in viewport background (25% scale)
        if (gLogoTex == 0) {
            int w=0,h=0,n=0;
            unsigned char* rgba = stbi_load_from_memory((const unsigned char*)logo_png_otf, (int)logo_png_otf_len, &w, &h, &n, 4);
            if (rgba) {
                gLogoW = w; gLogoH = h;
                glGenTextures(1, &gLogoTex);
                glBindTexture(GL_TEXTURE_2D, gLogoTex);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
                glBindTexture(GL_TEXTURE_2D, 0);
                stbi_image_free(rgba);
            }
        }
        if (gLogoTex != 0) {
            ImGuiViewport* vp = ImGui::GetMainViewport();
            ImVec2 center(vp->Pos.x + vp->Size.x * 0.5f, vp->Pos.y + vp->Size.y * 0.5f);
            float scale = 0.25f;
            ImVec2 sz((float)gLogoW * scale, (float)gLogoH * scale);
            ImVec2 pmin(center.x - sz.x * 0.5f, center.y - sz.y * 0.5f);
            ImVec2 pmax(center.x + sz.x * 0.5f, center.y + sz.y * 0.5f);
            ImGui::GetBackgroundDrawList()->AddImage((ImTextureID)(intptr_t)gLogoTex, pmin, pmax);
        }
        static bool showMain = true;
        // Global top menu bar (viewport)
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("Repository")) {
                if (ImGui::MenuItem("Refresh repos", "Ctrl+Shift+R")) ui::refreshRepos(ui);
                if (ImGui::MenuItem("New repo", "Ctrl+Shift+N")) ImGui::OpenPopup("init_repo");
                if (ImGui::MenuItem("Rename repo", "Ctrl+E", false, !ui.selectedRepo.empty())) {
                    std::snprintf(ui.renameRepoBuffer, sizeof(ui.renameRepoBuffer), "%s", ui.selectedRepo.c_str());
                    ui.showRenameRepoModal = true;
                }
                if (ImGui::MenuItem("Delete repo", "Ctrl+Del", false, !ui.selectedRepo.empty())) ui.confirmDeleteRepo = true;
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Items")) {
                if (ImGui::MenuItem("Add item", "Ctrl+N", false, !ui.selectedRepo.empty())) ui.showAddModal = true;
                if (ImGui::MenuItem("Rescan", "F5", false, !ui.selectedRepo.empty())) ui.requestRescan = true;
                if (ImGui::MenuItem("Verify", nullptr, false, !ui.selectedRepo.empty())) ImGui::OpenPopup("verify_popup");
                if (ImGui::MenuItem("Filters & Sorting", "Ctrl+F", ui.showFiltersWindow, !ui.selectedRepo.empty())) ui.showFiltersWindow = true;
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("GitHub")) {
                if (ImGui::MenuItem("Open GitHub Manager", "Ctrl+G")) ui.showGitHubWindow = true;
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Help")) {
                if (ImGui::MenuItem("About")) ui.showAboutWindow = true;
                if (ImGui::MenuItem("Help", "F1")) ui.showHelpWindow = true;
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }
        ui::menus::drawMain(ui, &showMain);
        ui::menus::drawGitHub(ui);
        ui::menus::drawFiltersWindow(ui);
        ui::menus::drawAboutWindow(ui);
        ui::menus::drawHelpWindow(ui);
        if (!showMain) { glfwSetWindowShouldClose(window, 1); }
        ImGui::Render();
        int display_w, display_h; glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }
    if (gLogoTex != 0) { glDeleteTextures(1, &gLogoTex); gLogoTex = 0; }
    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    if (gLogoTex != 0) { glBindTexture(GL_TEXTURE_2D, 0); glDeleteTextures(1, &gLogoTex); gLogoTex = 0; glFinish(); }
    ImGui::DestroyContext();
    glfwMakeContextCurrent(NULL);
    glfwDestroyWindow(window);
    glfwTerminate();
    if (isWSL) { std::_Exit(0); }
#endif
    return 0;
}

#ifdef _WIN32
int APIENTRY WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    return RunGui();
}
#else
int main()
{
    return RunGui();
}
#endif


