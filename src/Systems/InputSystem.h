#pragma once

#include <GLFW/glfw3.h>
#include <unordered_map>

class InputSystem {
public:
    InputSystem(GLFWwindow* window);
    
    void update();
    bool isKeyPressed(int key) const;
    bool isKeyJustPressed(int key) const;
    bool isKeyReleased(int key) const;

private:
    GLFWwindow* m_window;
    std::unordered_map<int, bool> m_previousKeyState;
    std::unordered_map<int, bool> m_currentKeyState;
};
