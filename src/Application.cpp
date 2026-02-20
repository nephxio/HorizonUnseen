#include "Application.h"
#include <chrono>

Application::Application() : m_running(false) {
    init();
}

Application::~Application() {
    cleanup();
}

void Application::init() {
    m_renderer = std::make_unique<VulkanRenderer>();
    m_renderer->init();

    m_inputSystem = std::make_unique<InputSystem>(m_renderer->getWindow());
    m_gameScene = std::make_unique<GameScene>();

    m_renderer->setGameScene(m_gameScene.get());

    m_running = true;
}

void Application::run() {
    auto lastTime = std::chrono::high_resolution_clock::now();

    while (m_running && !m_renderer->shouldClose()) {
        auto currentTime = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
        lastTime = currentTime;

        m_inputSystem->update();
        update(deltaTime);
        render();
    }

    m_renderer->waitIdle();
}

void Application::update(float deltaTime) {
    m_gameScene->update(deltaTime, *m_inputSystem);
}

void Application::render() {
    m_renderer->beginFrame();
    m_renderer->renderScene(*m_gameScene);
    m_renderer->renderUI();
    m_renderer->endFrame();
}

void Application::cleanup() {
    if (m_renderer) {
        m_renderer->cleanup();
    }
}
