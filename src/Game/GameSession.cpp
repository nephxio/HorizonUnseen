#include "GameSession.h"

#include <GLFW/glfw3.h>

GameSession::GameSession(GLFWwindow* window)
    : m_gameScene(std::make_unique<GameScene>()),
      m_inputSystem(std::make_unique<InputSystem>(window)) {
}

void GameSession::updateInput() {
    m_inputSystem->update();
}

void GameSession::updateScene(float deltaTime) {
    m_gameScene->update(deltaTime, *m_inputSystem);
}

void GameSession::update(float deltaTime) {
    updateInput();
    updateScene(deltaTime);
}
