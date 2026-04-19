#pragma once

#include "Renderer/VulkanRenderer.h"
#include "Game/GameSession.h"
#include "Utility/FrameTimer.h"
#include <memory>

class Application {
public:
    Application();
    ~Application();

    void run();

private:
    void init();
    void mainLoop();
    void cleanup();
    void update(float deltaTime);

    std::unique_ptr<VulkanRenderer> m_renderer;
    std::unique_ptr<GameSession> m_gameSession;
    FrameTimer m_frameTimer;

    bool m_running;
    bool m_lastTildeState = false;
};
