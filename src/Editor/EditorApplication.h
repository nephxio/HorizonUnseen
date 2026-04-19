#pragma once

#include "Renderer/VulkanContext.h"
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>

class EditorApplication {
public:
    EditorApplication();
    ~EditorApplication();

    void run();

private:
    void init();
    void initWindow();
    void initVulkan();
    void initImGui();
    void createCommandBuffers();
    void createSyncObjects();
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    void renderUI();
    void renderDockspace();
    void renderHierarchyPanel();
    void renderInspectorPanel();
    void renderContentBrowserPanel();
    void renderViewportPanel();
    void beginFrame();
    void endFrame();
    void cleanup();

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

    int m_selectedItem = -1;
    bool m_showHierarchy = true;
    bool m_showInspector = true;
    bool m_showContentBrowser = true;
    bool m_showViewport = true;

    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
    const uint32_t WIDTH = 1280;
    const uint32_t HEIGHT = 720;
};
