#include "Application.h"
#include "Debug/DebugConsole.h"
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

    std::cout << "Creating shared game session..." << std::endl;
    m_gameSession = std::make_unique<GameSession>(m_renderer->getWindow());

    std::cout << "Setting game scene in renderer..." << std::endl;
    m_renderer->setGameScene(&m_gameSession->getScene());

    std::cout << "Initialization complete!" << std::endl;
    m_running = true;
}

void Application::run() {
    while (m_running && !m_renderer->shouldClose()) {
        update(m_frameTimer.tick());
        m_renderer->renderFrame();
    }

    m_renderer->waitIdle();
}

void Application::update(float deltaTime) {
    m_gameSession->updateInput();

    // Toggle debug console with tilde (~) key
    bool currentTildeState = m_gameSession->getInputSystem().isKeyPressed(GLFW_KEY_GRAVE_ACCENT);
    if (currentTildeState && !m_lastTildeState) {
        DebugConsole::getInstance().toggleConsole();
    }
    m_lastTildeState = currentTildeState;

    m_gameSession->updateScene(deltaTime);
}

void Application::cleanup() {
    if (m_renderer) {
        m_renderer->cleanup();
    }
}
