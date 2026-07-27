#pragma once

// Shared colours and small drawing helpers for the UI, so the HUD, menus and
// toasts all look like parts of the same game.

#include <imgui.h>

namespace hu {
namespace theme {

// Palette. Cool blues for the player/ship systems, amber for charge, red for
// damage and breakage.
constexpr ImVec4 Accent      { 0.35f, 0.78f, 1.00f, 1.00f };
constexpr ImVec4 AccentDim   { 0.18f, 0.42f, 0.58f, 1.00f };
constexpr ImVec4 Charge      { 1.00f, 0.78f, 0.25f, 1.00f };
constexpr ImVec4 ChargeFull  { 1.00f, 0.95f, 0.55f, 1.00f };
constexpr ImVec4 Health      { 0.30f, 0.85f, 0.45f, 1.00f };
constexpr ImVec4 Danger      { 0.95f, 0.28f, 0.28f, 1.00f };
constexpr ImVec4 Broken      { 0.35f, 0.12f, 0.12f, 1.00f };
constexpr ImVec4 Locked      { 0.40f, 0.40f, 0.45f, 1.00f };
constexpr ImVec4 TextDim     { 0.62f, 0.66f, 0.72f, 1.00f };
constexpr ImVec4 Panel       { 0.05f, 0.07f, 0.10f, 0.82f };
constexpr ImVec4 Secret      { 0.85f, 0.55f, 1.00f, 1.00f };

// Applies the game's global ImGui style. Call once after ImGui context creation.
void apply();

// Colour for a weapon, used consistently by the HUD and the menus.
ImVec4 weaponColor(int weaponIndex);

// Linear blend, for pulsing/flashing UI elements.
inline ImVec4 mix(const ImVec4& a, const ImVec4& b, float t) {
    return ImVec4(a.x + (b.x - a.x) * t,
                  a.y + (b.y - a.y) * t,
                  a.z + (b.z - a.z) * t,
                  a.w + (b.w - a.w) * t);
}

inline ImVec4 withAlpha(const ImVec4& c, float alpha) {
    return ImVec4(c.x, c.y, c.z, alpha);
}

} // namespace theme
} // namespace hu
