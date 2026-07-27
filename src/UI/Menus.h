#pragma once

#include "UI/UiModel.h"

#include <string>

namespace hu {

// All full-screen menus.
//
// Every draw* function is pure with respect to game state: it renders from the
// model it is given and returns the action the player chose. The application
// owns the consequences.
class Menus {
public:
    // Title screen. Bullet Hell is gated on progress.bulletHellUnlocked and
    // shows how many secrets remain when locked.
    MenuAction drawMainMenu(const ProgressModel& progress, float viewportWidth, float viewportHeight,
                            float deltaTime);

    MenuAction drawPauseMenu(float viewportWidth, float viewportHeight);

    MenuAction drawGameOver(const HudModel& model, float viewportWidth, float viewportHeight);

    MenuAction drawLevelComplete(const HudModel& model, float viewportWidth, float viewportHeight);

    // Per-level secret browser. Hints are only revealed for levels already
    // played, so a first run stays a discovery.
    MenuAction drawSecretsScreen(const ProgressModel& progress, float viewportWidth, float viewportHeight);

    MenuAction drawOptions(float viewportWidth, float viewportHeight);

private:
    void drawTitle(const char* title, const char* subtitle, float viewportWidth, float y);
    void beginCenteredPanel(const char* id, float viewportWidth, float viewportHeight,
                            float panelWidth, float panelHeight);
    void endCenteredPanel();

    float m_titlePulse = 0.0f;
    int m_selectedLevel = 0;
};

} // namespace hu
