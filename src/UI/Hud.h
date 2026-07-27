#pragma once

#include "UI/UiModel.h"

#include <deque>

namespace hu {

// The in-game heads-up display.
//
// Draws as a transparent, input-transparent overlay across the whole viewport
// so it never steals mouse/keyboard focus from gameplay.
class Hud {
public:
    void draw(const HudModel& model, float viewportWidth, float viewportHeight, float deltaTime);

    // Toasts are owned by the HUD so callers just fire and forget.
    void pushToast(const Toast& toast);
    void updateToasts(float deltaTime);
    void clearToasts();

private:
    void drawEnergyCells(const HudModel& model, float x, float y, float width);
    void drawWeaponBar(const HudModel& model, float x, float y);
    void drawSuperweaponReadout(const HudModel& model, float x, float y);
    void drawDamagePressure(const HudModel& model, float x, float y, float width);
    void drawLevelProgress(const HudModel& model, float viewportWidth);
    void drawBossBar(const HudModel& model, float viewportWidth, float viewportHeight);
    void drawToasts(float viewportWidth, float viewportHeight);

    std::deque<Toast> m_toasts;
    float m_pulseTime = 0.0f;   // Drives charged-cell / ready-to-fire pulsing.
};

} // namespace hu
