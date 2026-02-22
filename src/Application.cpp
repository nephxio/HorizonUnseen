#include "Application.h"
#include "Debug/DebugConsole.h"
#include <chrono>
#include <iostream>

Application::Application() : m_running(false) {
    init();
}

Application::~Application() {
    cleanup();
}

void Application::init() {
    std::cout << "Creating renderer..." << std::endl;
    m_renderer = std::make_unique<VulkanRenderer>();

    std::cout << "Initializing renderer..." << std::endl;
    m_renderer->init();

    std::cout << "Creating input system..." << std::endl;
    m_inputSystem = std::make_unique<InputSystem>(m_renderer->getWindow());

    std::cout << "Creating game scene..." << std::endl;
    m_gameScene = std::make_unique<GameScene>();

    std::cout << "Setting game scene in renderer..." << std::endl;
    m_renderer->setGameScene(m_gameScene.get());

    std::cout << "Initialization complete!" << std::endl;
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
    // Toggle debug console with tilde (~) key
    bool currentTildeState = m_inputSystem->isKeyPressed(GLFW_KEY_GRAVE_ACCENT);
    if (currentTildeState && !m_lastTildeState) {
        DebugConsole::getInstance().toggleConsole();
    }
    m_lastTildeState = currentTildeState;

    m_gameScene->update(deltaTime, *m_inputSystem);
}

void Application::render() {
    // Prepare ImGui data first
    m_renderer->renderUI();

    // Then render the frame (which will include ImGui)
    m_renderer->beginFrame();
    m_renderer->renderScene(*m_gameScene);
    m_renderer->endFrame();
}

void Application::cleanup() {
    if (m_renderer) {
        m_renderer->cleanup();
    }
}
