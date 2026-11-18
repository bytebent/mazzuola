#include "ui/theme.hpp"

namespace mazz {

void applyTheme() {
    ImGui::StyleColorsLight();
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 4;
    s.ChildRounding = 4;
    s.FrameRounding = 4;
    s.PopupRounding = 4;
    s.GrabRounding = 4;
    s.TabRounding = 4;
    s.ScrollbarRounding = 6;
    s.FrameBorderSize = 1;
    s.WindowBorderSize = 1;
    s.TabBorderSize = 0;
    s.FramePadding = ImVec2(7, 4);
    s.ItemSpacing = ImVec2(8, 6);
    s.WindowPadding = ImVec2(10, 10);
    s.DockingSeparatorSize = 3;

    ImVec4* c = s.Colors;
    const ImVec4 white{1, 1, 1, 1};
    const ImVec4 panel{0.955f, 0.957f, 0.965f, 1};
    const ImVec4 frame{0.995f, 0.995f, 1.0f, 1};
    const ImVec4 border{0.78f, 0.79f, 0.82f, 1};
    c[ImGuiCol_Text] = ImVec4(0.13f, 0.14f, 0.16f, 1);
    c[ImGuiCol_TextDisabled] = ImVec4(0.58f, 0.59f, 0.62f, 1);
    c[ImGuiCol_WindowBg] = panel;
    c[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_PopupBg] = white;
    c[ImGuiCol_MenuBarBg] = ImVec4(0.93f, 0.935f, 0.945f, 1);
    c[ImGuiCol_Border] = border;
    c[ImGuiCol_FrameBg] = frame;
    c[ImGuiCol_FrameBgHovered] = ImVec4(1.0f, 0.95f, 0.95f, 1);
    c[ImGuiCol_FrameBgActive] = ImVec4(1.0f, 0.92f, 0.92f, 1);
    c[ImGuiCol_TitleBg] = ImVec4(0.90f, 0.905f, 0.92f, 1);
    c[ImGuiCol_TitleBgActive] = ImVec4(0.88f, 0.885f, 0.90f, 1);
    c[ImGuiCol_TitleBgCollapsed] = ImVec4(0.90f, 0.905f, 0.92f, 1);
    c[ImGuiCol_CheckMark] = theme::kAccent;
    c[ImGuiCol_SliderGrab] = theme::kAccent;
    c[ImGuiCol_SliderGrabActive] = theme::kAccentActive;
    c[ImGuiCol_Button] = ImVec4(0.90f, 0.905f, 0.92f, 1);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.98f, 0.86f, 0.87f, 1);
    c[ImGuiCol_ButtonActive] = ImVec4(0.95f, 0.76f, 0.78f, 1);
    c[ImGuiCol_Header] = theme::kAccentSoft;
    c[ImGuiCol_HeaderHovered] = theme::kAccentSoftHover;
    c[ImGuiCol_HeaderActive] = ImVec4(0.82f, 0.20f, 0.24f, 0.40f);
    c[ImGuiCol_Separator] = border;
    c[ImGuiCol_SeparatorHovered] = theme::kAccentHover;
    c[ImGuiCol_SeparatorActive] = theme::kAccent;
    c[ImGuiCol_ResizeGrip] = ImVec4(0.82f, 0.20f, 0.24f, 0.15f);
    c[ImGuiCol_ResizeGripHovered] = ImVec4(0.82f, 0.20f, 0.24f, 0.45f);
    c[ImGuiCol_ResizeGripActive] = theme::kAccent;
    c[ImGuiCol_Tab] = ImVec4(0.90f, 0.905f, 0.92f, 1);
    c[ImGuiCol_TabHovered] = ImVec4(0.98f, 0.86f, 0.87f, 1);
    c[ImGuiCol_TabSelected] = white;
    c[ImGuiCol_TabSelectedOverline] = theme::kAccent;
    c[ImGuiCol_TabDimmed] = ImVec4(0.92f, 0.925f, 0.935f, 1);
    c[ImGuiCol_TabDimmedSelected] = ImVec4(0.975f, 0.975f, 0.98f, 1);
    c[ImGuiCol_TabDimmedSelectedOverline] = ImVec4(0.82f, 0.20f, 0.24f, 0.5f);
    c[ImGuiCol_DockingPreview] = ImVec4(0.82f, 0.20f, 0.24f, 0.35f);
    c[ImGuiCol_DockingEmptyBg] = panel;
    c[ImGuiCol_TextSelectedBg] = ImVec4(0.82f, 0.20f, 0.24f, 0.25f);
    c[ImGuiCol_NavCursor] = theme::kAccent;
    c[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ScrollbarGrab] = ImVec4(0.75f, 0.76f, 0.79f, 1);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.65f, 0.66f, 0.70f, 1);
    c[ImGuiCol_ScrollbarGrabActive] = theme::kAccent;
    c[ImGuiCol_TableHeaderBg] = ImVec4(0.91f, 0.915f, 0.93f, 1);
    c[ImGuiCol_TableBorderStrong] = border;
    c[ImGuiCol_TableBorderLight] = ImVec4(0.86f, 0.87f, 0.89f, 1);
    c[ImGuiCol_TableRowBgAlt] = ImVec4(0, 0, 0, 0.025f);
}

} // namespace mazz
