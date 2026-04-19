#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include <memory>
#include <vector>

#include "Renderer/VulkanContext.h"

struct VulkanWindowSettings {
    const char* title = "Horizon Unseen";
    uint32_t width = 1280;
    uint32_t height = 720;
    bool resizable = false;
};

class IVulkanFrameClient {
public:
    virtual ~IVulkanFrameClient() = default;

    virtual VkClearValue getClearColor() const = 0;
    virtual void recordScene(VkCommandBuffer commandBuffer) = 0;
    virtual void renderUi() = 0;
};

class VulkanFrameHost {
public:
    VulkanFrameHost();
    ~VulkanFrameHost();

    void init(const VulkanWindowSettings& settings);
    void renderFrame(IVulkanFrameClient& client);
    void waitIdle();
    void cleanup();

    bool shouldClose() const;
    GLFWwindow* getWindow() const { return m_window; }
    VulkanContext& getContext() { return *m_context; }
    const VulkanContext& getContext() const { return *m_context; }

private:
    void initWindow();
    void initVulkan();
    void initImGui();
    void createCommandBuffers();
    void createSyncObjects();
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex, IVulkanFrameClient& client);

    VulkanWindowSettings m_settings{};
    GLFWwindow* m_window = nullptr;
    std::unique_ptr<VulkanContext> m_context;

    std::vector<VkCommandBuffer> m_commandBuffers;
    std::vector<VkSemaphore> m_imageAvailableSemaphores;
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
    std::vector<VkFence> m_inFlightFences;

    VkDescriptorPool m_imguiDescriptorPool = VK_NULL_HANDLE;

    uint32_t m_currentFrame = 0;
    uint32_t m_imageIndex = 0;
    bool m_isCleanedUp = false;
    bool m_glfwInitialized = false;
    bool m_windowCreated = false;
    bool m_contextCreated = false;
    bool m_imguiInitialized = false;
    bool m_syncObjectsCreated = false;

    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
};
