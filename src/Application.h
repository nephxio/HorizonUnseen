#pragma once

// Top-level game application.
//
// Owns the window/renderer, the playable world and the UI, and runs the state
// machine that moves between the menus and play. This is the only place that
// depends on gameplay, UI and rendering at once; each of those three knows
// nothing about the others.

#include "Core/DrawList.h"
#include "Core/GameTypes.h"
#include "Gameplay/GameWorld.h"
#include "Renderer/VulkanRenderer.h"
#include "UI/DebugOverlay.h"
#include "UI/Hud.h"
#include "UI/Menus.h"
#include "UI/UiModel.h"
#include "Utility/FrameTimer.h"

#include <memory>

class InputSystem;

class Application {
public:
    Application();
    ~Application();

    void run();

private:
    void init();
    void cleanup();

    void update(float deltaTime);
    void drawUi();

    // State transitions.
    void startLevel(hu::DifficultyMode mode);
    void returnToMenu();
    void applyMenuAction(hu::MenuAction action);

    // View-model construction. The UI never touches gameplay objects directly.
    void buildHudModel();
    void buildProgressModel();
    void buildDebugStats();

    std::unique_ptr<VulkanRenderer> m_renderer;
    std::unique_ptr<InputSystem> m_input;
    std::unique_ptr<hu::GameWorld> m_world;

    hu::Hud m_hud;
    hu::Menus m_menus;
    hu::DebugOverlay m_debugOverlay;

    hu::HudModel m_hudModel;
    hu::ProgressModel m_progressModel;
    hu::DebugStats m_debugStats;
    hu::DrawList m_drawList;

    hu::GameStateId m_state = hu::GameStateId::MainMenu;
    hu::DifficultyMode m_difficulty = hu::DifficultyMode::Normal;

    FrameTimer m_frameTimer;
    float m_lastDeltaTime = 0.0f;
    unsigned long long m_frameIndex = 0;

    bool m_running = false;
    bool m_debugOpen = false;

    // Smoothed so the debug overlay's FPS readout is legible.
    float m_fpsAccumulator = 0.0f;
    int m_fpsSamples = 0;
    float m_displayFps = 0.0f;
};
