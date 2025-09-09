#include "about_window.h"
#include "imgui.h"

namespace ui { namespace menus {

static void CenterNextWindow() {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImVec2 sz(480, 260);
    ImGui::SetNextWindowSize(sz, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + vp->Size.x * 0.5f, vp->Pos.y + vp->Size.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
}

void drawAboutWindow(State& ui)
{
    if (!ui.showAboutWindow) return;
    CenterNextWindow();
    if (ImGui::Begin("About RepoMan", &ui.showAboutWindow, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("RepoMan - Repository Manager");
        ImGui::Separator();
        ImGui::Text("Version: 1.0");
        ImGui::Text("Author: UniteHub");
        ImGui::Text("License: MIT");
        ImGui::Separator();
        ImGui::TextWrapped("RepoMan is a tool for managing local content repositories with GitHub integration, metadata editing, and batch operations.");
        if (ImGui::Button("Close")) ui.showAboutWindow = false;
        ImGui::End();
    }
}

} }


