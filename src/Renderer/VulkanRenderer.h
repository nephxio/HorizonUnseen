#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>

class GameScene;
class VulkanContext;

class VulkanRenderer {
public:
    VulkanRenderer();
    ~VulkanRenderer();

    void init();
    void cleanup();
    
    void beginFrame();
    void renderScene(const GameScene& scene);
    void renderUI();
    void endFrame();

    bool shouldClose() const;
    void waitIdle();
    GLFWwindow* getWindow() const { return m_window; }

private:
    void initWindow();
    void initVulkan();
    void initImGui();

    GLFWwindow* m_window;
    std::unique_ptr<VulkanContext> m_context;
    
    const uint32_t WIDTH = 1280;
    const uint32_t HEIGHT = 720;
};
