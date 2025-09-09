#include "help_window.h"
#include "imgui.h"

namespace ui { namespace menus {

static void CenterNextWindow() {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImVec2 sz(720, 520);
    ImGui::SetNextWindowSize(sz, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + vp->Size.x * 0.5f, vp->Pos.y + vp->Size.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
}

void drawHelpWindow(State& ui)
{
    if (!ui.showHelpWindow) return;
    CenterNextWindow();
    if (ImGui::Begin("Help - RepoMan", &ui.showHelpWindow, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("User Guide");
        ImGui::Separator();
        if (ImGui::CollapsingHeader("Getting Started", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::BulletText("Select a repository from the dropdown, or create a new one.");
            ImGui::BulletText("Use Add item to import files. Drag & drop is supported.");
        }
        if (ImGui::CollapsingHeader("Managing Items")) {
            ImGui::BulletText("Right-click an item for context actions (Edit Metadata, Copy Path, Open Folder, Remove).");
            ImGui::BulletText("Edit Metadata supports tag editing in key:value format, with ordering and removal.");
        }
        if (ImGui::CollapsingHeader("Batch Operations")) {
            ImGui::BulletText("Use Ctrl+Click to select multiple items.");
            ImGui::BulletText("Batch Edit Tags: add key:value to all selected, remove by key.");
            ImGui::BulletText("Batch Move: move files to a target folder inside repository.");
            ImGui::BulletText("Batch Delete: permanently deletes selected files.");
        }
        if (ImGui::CollapsingHeader("Filters & Sorting")) {
            ImGui::BulletText("Open Filters & Sorting to search by name/author/tag, filter by type, and sort by name/date/size.");
        }
        if (ImGui::CollapsingHeader("GitHub Manager")) {
            ImGui::BulletText("Login with a token, list repositories, clone/pull/push.");
            ImGui::BulletText("Context menu on a repo row allows renaming, visibility toggle, delete, copy name.");
            ImGui::BulletText("Compare with GitHub loads remote index.json and shows local/remote status per item.");
        }
        if (ImGui::CollapsingHeader("Keyboard & Tips")) {
            ImGui::BulletText("Ctrl+Click: multi-select in items table.");
            ImGui::BulletText("Filters window: adjust and close; settings persist for current session.");
        }
        ImGui::Separator();
        if (ImGui::Button("Close")) ui.showHelpWindow = false;
        ImGui::End();
    }
}

} }


