#include "InputSystem.h"

InputSystem::InputSystem(GLFWwindow* window) : m_window(window) {}

void InputSystem::update() {
    // Store previous frame's key states
    m_previousKeyState = m_currentKeyState;

    // Update current key states for keys we care about
    int keys[] = {
        GLFW_KEY_W, GLFW_KEY_A, GLFW_KEY_S, GLFW_KEY_D,
        GLFW_KEY_UP, GLFW_KEY_DOWN, GLFW_KEY_LEFT, GLFW_KEY_RIGHT,
        GLFW_KEY_SPACE, GLFW_KEY_ESCAPE, GLFW_KEY_GRAVE_ACCENT
    };
    for (int key : keys) {
        m_currentKeyState[key] = glfwGetKey(m_window, key) == GLFW_PRESS;
    }
}

bool InputSystem::isKeyPressed(int key) const {
    auto it = m_currentKeyState.find(key);
    return it != m_currentKeyState.end() && it->second;
}

bool InputSystem::isKeyJustPressed(int key) const {
    auto current = m_currentKeyState.find(key);
    auto previous = m_previousKeyState.find(key);
    
    bool currentlyPressed = (current != m_currentKeyState.end() && current->second);
    bool previouslyPressed = (previous != m_previousKeyState.end() && previous->second);
    
    return currentlyPressed && !previouslyPressed;
}

bool InputSystem::isKeyReleased(int key) const {
    auto current = m_currentKeyState.find(key);
    auto previous = m_previousKeyState.find(key);
    
    bool currentlyPressed = (current != m_currentKeyState.end() && current->second);
    bool previouslyPressed = (previous != m_previousKeyState.end() && previous->second);
    
    return !currentlyPressed && previouslyPressed;
}
