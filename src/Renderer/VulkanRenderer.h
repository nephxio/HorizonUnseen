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

    void setGameScene(const GameScene* scene) { m_gameScene = scene; }

private:
    void initWindow();
    void initVulkan();
    void initImGui();
    void createCommandBuffers();
    void createSyncObjects();
    void createVertexBuffer();
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

    GLFWwindow* m_window;
    std::unique_ptr<VulkanContext> m_context;
    const GameScene* m_gameScene = nullptr;

    std::vector<VkCommandBuffer> m_commandBuffers;
    std::vector<VkSemaphore> m_imageAvailableSemaphores;
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
    std::vector<VkFence> m_inFlightFences;

    VkBuffer m_vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_vertexBufferMemory = VK_NULL_HANDLE;

    uint32_t m_currentFrame = 0;
    uint32_t m_imageIndex = 0;
    bool m_isCleanedUp = false;

    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
    const uint32_t WIDTH = 1280;
    const uint32_t HEIGHT = 720;
};
