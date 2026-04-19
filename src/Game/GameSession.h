#pragma once

#include "GameScene.h"
#include "Systems/InputSystem.h"

#include <memory>

class GameSession {
public:
    explicit GameSession(GLFWwindow* window);

    void updateInput();
    void updateScene(float deltaTime);
    void update(float deltaTime);

    GameScene& getScene() { return *m_gameScene; }
    const GameScene& getScene() const { return *m_gameScene; }

    InputSystem& getInputSystem() { return *m_inputSystem; }
    const InputSystem& getInputSystem() const { return *m_inputSystem; }

private:
    std::unique_ptr<GameScene> m_gameScene;
    std::unique_ptr<InputSystem> m_inputSystem;
};
