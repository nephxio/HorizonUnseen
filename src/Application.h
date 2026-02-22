#pragma once

#include "Renderer/VulkanRenderer.h"
#include "Game/GameScene.h"
#include "Systems/InputSystem.h"
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
    void render();

    std::unique_ptr<VulkanRenderer> m_renderer;
    std::unique_ptr<GameScene> m_gameScene;
    std::unique_ptr<InputSystem> m_inputSystem;

    bool m_running;
    bool m_lastTildeState = false;
};
