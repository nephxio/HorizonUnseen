#include "UI/UiTheme.h"

namespace hu {
namespace theme {

void apply() {
    ImGuiStyle& style = ImGui::GetStyle();

    style.WindowRounding = 4.0f;
    style.FrameRounding = 3.0f;
    style.GrabRounding = 3.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.WindowPadding = ImVec2(14.0f, 12.0f);
    style.FramePadding = ImVec2(10.0f, 6.0f);
    style.ItemSpacing = ImVec2(9.0f, 8.0f);
    style.ScrollbarSize = 12.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg]        = ImVec4(0.04f, 0.05f, 0.08f, 0.94f);
    colors[ImGuiCol_ChildBg]         = ImVec4(0.06f, 0.08f, 0.11f, 0.60f);
    colors[ImGuiCol_PopupBg]         = ImVec4(0.04f, 0.05f, 0.08f, 0.98f);
    colors[ImGuiCol_Border]          = ImVec4(0.18f, 0.30f, 0.40f, 0.65f);
    colors[ImGuiCol_FrameBg]         = ImVec4(0.10f, 0.14f, 0.19f, 0.90f);
    colors[ImGuiCol_FrameBgHovered]  = ImVec4(0.16f, 0.24f, 0.32f, 0.95f);
    colors[ImGuiCol_FrameBgActive]   = ImVec4(0.20f, 0.34f, 0.45f, 0.98f);
    colors[ImGuiCol_TitleBg]         = ImVec4(0.06f, 0.09f, 0.13f, 1.00f);
    colors[ImGuiCol_TitleBgActive]   = ImVec4(0.10f, 0.18f, 0.26f, 1.00f);
    colors[ImGuiCol_Button]          = ImVec4(0.12f, 0.20f, 0.28f, 0.95f);
    colors[ImGuiCol_ButtonHovered]   = ImVec4(0.20f, 0.38f, 0.52f, 1.00f);
    colors[ImGuiCol_ButtonActive]    = ImVec4(0.28f, 0.52f, 0.70f, 1.00f);
    colors[ImGuiCol_Header]          = ImVec4(0.14f, 0.24f, 0.34f, 0.90f);
    colors[ImGuiCol_HeaderHovered]   = ImVec4(0.20f, 0.36f, 0.50f, 0.95f);
    colors[ImGuiCol_HeaderActive]    = ImVec4(0.26f, 0.46f, 0.62f, 1.00f);
    colors[ImGuiCol_Separator]       = ImVec4(0.18f, 0.30f, 0.40f, 0.60f);
    colors[ImGuiCol_PlotHistogram]   = Accent;
    colors[ImGuiCol_Text]            = ImVec4(0.90f, 0.93f, 0.96f, 1.00f);
    colors[ImGuiCol_TextDisabled]    = TextDim;
}

ImVec4 weaponColor(int weaponIndex) {
    // Matches the order of hu::WeaponType: Bullet, Spread, Missile, Laser.
    switch (weaponIndex) {
        case 0:  return ImVec4(0.95f, 0.85f, 0.40f, 1.0f);  // Bullet  - yellow
        case 1:  return ImVec4(0.45f, 0.90f, 0.55f, 1.0f);  // Spread  - green
        case 2:  return ImVec4(1.00f, 0.55f, 0.35f, 1.0f);  // Missile - orange
        case 3:  return ImVec4(0.45f, 0.80f, 1.00f, 1.0f);  // Laser   - cyan
        default: return TextDim;
    }
}

} // namespace theme
} // namespace hu
