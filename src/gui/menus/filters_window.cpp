#include "filters_window.h"
#include "imgui.h"

namespace ui { namespace menus {

static void CenterNextWindow() {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImVec2 sz(520, 240);
    ImGui::SetNextWindowSize(sz, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + vp->Size.x * 0.5f, vp->Pos.y + vp->Size.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
}

void drawFiltersWindow(State& ui)
{
    if (!ui.showFiltersWindow) return;
    CenterNextWindow();
    if (ImGui::Begin("Filters & Sorting", &ui.showFiltersWindow, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Search/Filter");
        ImGui::Separator();
        ImGui::InputText("Name", ui.filterName, IM_ARRAYSIZE(ui.filterName));
        ImGui::InputText("Author", ui.filterAuthor, IM_ARRAYSIZE(ui.filterAuthor));
        ImGui::InputText("Tag (key or key:value)", ui.filterTag, IM_ARRAYSIZE(ui.filterTag));
        ImGui::Separator();
        ImGui::Text("Types");
        ImGui::Checkbox("pk3", &ui.filterPK3); ImGui::SameLine();
        ImGui::Checkbox("cfg", &ui.filterCFG); ImGui::SameLine();
        ImGui::Checkbox("exe", &ui.filterEXE);
        ImGui::Separator();
        ImGui::Text("Sorting");
        const char* sortFields[] = {"Name","Updated","Size"};
        ImGui::Combo("Field", &ui.sortField, sortFields, 3); ImGui::SameLine();
        ImGui::Checkbox("Descending", &ui.sortDesc);
        ImGui::Separator();
        if (ImGui::Button("Close")) { ui.showFiltersWindow = false; }
        ImGui::End();
    }
}

} }


