#ifndef GUI_THEMES_RED_H
#define GUI_THEMES_RED_H

#include "imgui.h"

namespace ui { namespace themes {

inline void applyRed()
{
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = ImGui::GetStyle().Colors;
    
    colors[ImGuiCol_Text]                  = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_TextDisabled]          = ImVec4(0.60f, 0.45f, 0.45f, 1.00f);
    colors[ImGuiCol_WindowBg]              = ImVec4(0.10f, 0.07f, 0.08f, 0.90f);
    colors[ImGuiCol_ChildBg]               = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_PopupBg]               = ImVec4(0.16f, 0.09f, 0.10f, 0.95f);
    colors[ImGuiCol_Border]                = ImVec4(0.30f, 0.12f, 0.12f, 0.60f);
    colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.20f);
    colors[ImGuiCol_FrameBg]               = ImVec4(0.18f, 0.08f, 0.09f, 0.80f);
    colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.32f, 0.11f, 0.12f, 0.85f);
    colors[ImGuiCol_FrameBgActive]         = ImVec4(0.45f, 0.14f, 0.15f, 1.00f);
    colors[ImGuiCol_TitleBg]               = ImVec4(0.18f, 0.06f, 0.07f, 1.00f);
    colors[ImGuiCol_TitleBgActive]         = ImVec4(0.28f, 0.08f, 0.09f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.10f, 0.04f, 0.05f, 1.00f);
    colors[ImGuiCol_MenuBarBg]             = ImVec4(0.14f, 0.08f, 0.08f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.10f, 0.05f, 0.05f, 0.54f);
    colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.40f, 0.15f, 0.15f, 0.70f);
    colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.52f, 0.18f, 0.18f, 0.80f);
    colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.68f, 0.22f, 0.22f, 0.85f);
    colors[ImGuiCol_CheckMark]             = ImVec4(0.95f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_SliderGrab]            = ImVec4(0.85f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.95f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_Button]                = ImVec4(0.26f, 0.09f, 0.10f, 0.85f);
    colors[ImGuiCol_ButtonHovered]         = ImVec4(0.38f, 0.12f, 0.13f, 0.90f);
    colors[ImGuiCol_ButtonActive]          = ImVec4(0.55f, 0.16f, 0.17f, 1.00f);
    colors[ImGuiCol_Header]                = ImVec4(0.20f, 0.07f, 0.08f, 0.75f);
    colors[ImGuiCol_HeaderHovered]         = ImVec4(0.30f, 0.10f, 0.11f, 0.90f);
    colors[ImGuiCol_HeaderActive]          = ImVec4(0.45f, 0.14f, 0.15f, 1.00f);
    colors[ImGuiCol_Separator]             = ImVec4(0.40f, 0.14f, 0.15f, 0.70f);
    colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.55f, 0.18f, 0.19f, 0.78f);
    colors[ImGuiCol_SeparatorActive]       = ImVec4(0.80f, 0.22f, 0.23f, 1.00f);
    colors[ImGuiCol_ResizeGrip]            = ImVec4(0.30f, 0.10f, 0.11f, 0.40f);
    colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.50f, 0.16f, 0.17f, 0.70f);
    colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.80f, 0.22f, 0.23f, 1.00f);
    colors[ImGuiCol_Tab]                   = ImVec4(0.20f, 0.07f, 0.08f, 0.85f);
    colors[ImGuiCol_TabHovered]            = ImVec4(0.35f, 0.11f, 0.12f, 1.00f);
    colors[ImGuiCol_TabActive]             = ImVec4(0.45f, 0.14f, 0.15f, 1.00f);
    colors[ImGuiCol_TabUnfocused]          = ImVec4(0.16f, 0.06f, 0.07f, 0.85f);
    colors[ImGuiCol_TabUnfocusedActive]    = ImVec4(0.32f, 0.11f, 0.12f, 1.00f);
    colors[ImGuiCol_PlotLines]             = ImVec4(1.00f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered]      = ImVec4(1.00f, 0.35f, 0.35f, 1.00f);
    colors[ImGuiCol_PlotHistogram]         = ImVec4(1.00f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(1.00f, 0.45f, 0.45f, 1.00f);
    colors[ImGuiCol_TableHeaderBg]         = ImVec4(0.16f, 0.06f, 0.07f, 0.85f);
    colors[ImGuiCol_TableBorderStrong]     = ImVec4(0.16f, 0.06f, 0.07f, 0.85f);
    colors[ImGuiCol_TableBorderLight]      = ImVec4(0.30f, 0.10f, 0.11f, 0.35f);
    colors[ImGuiCol_TableRowBg]            = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]         = ImVec4(0.80f, 0.30f, 0.30f, 0.06f);
    colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.60f, 0.20f, 0.20f, 0.85f);
    colors[ImGuiCol_DragDropTarget]        = ImVec4(1.00f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_NavHighlight]          = ImVec4(1.00f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 0.20f, 0.20f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg]     = ImVec4(1.00f, 0.20f, 0.20f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.80f, 0.10f, 0.10f, 0.35f);

    style.WindowPadding     = ImVec2(8.0f, 8.0f);
    style.FramePadding      = ImVec2(5.0f, 5.0f);
    style.CellPadding       = ImVec2(6.0f, 6.0f);
    style.ItemSpacing       = ImVec2(6.0f, 6.0f);
    style.ItemInnerSpacing  = ImVec2(5.0f, 5.0f);
    style.TouchExtraPadding = ImVec2(0.0f, 0.0f);
    style.IndentSpacing     = 25.0f;
    style.ScrollbarSize     = 15.0f;
    style.GrabMinSize       = 10.0f;
    style.WindowBorderSize  = 1.0f;
    style.ChildBorderSize   = 1.0f;
    style.PopupBorderSize   = 1.0f;
    style.FrameBorderSize   = 1.0f;
    style.TabBorderSize     = 1.0f;
    style.WindowRounding    = 7.0f;
    style.ChildRounding     = 4.0f;
    style.FrameRounding     = 3.0f;
    style.PopupRounding     = 4.0f;
    style.ScrollbarRounding = 9.0f;
    style.GrabRounding      = 3.0f;
    style.LogSliderDeadzone = 4.0f;
    style.TabRounding       = 4.0f;
}

} }

#endif


