#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <memory>

#include "Game/GameSession.h"
#include "Rendering/SceneRenderer.h"
#include "Rendering/VulkanFrameHost.h"
#include "Utility/FrameTimer.h"

class EditorApplication : public IVulkanFrameClient {
public:
    EditorApplication();
    ~EditorApplication();

    void run();

private:
    void init();
    void initViewportResources();
    void updateViewportTexture();
    void cleanupViewportResources();
    void renderFrame();
    VkClearValue getClearColor() const override;
    void recordScene(VkCommandBuffer commandBuffer) override;
    void renderUi() override;
    void renderDockspace();
    void renderHierarchyPanel();
    void renderInspectorPanel();
    void renderContentBrowserPanel();
    void renderViewportPanel();
    void cleanup();

    VulkanFrameHost m_frameHost;
    SceneRenderer m_sceneRenderer;
    std::unique_ptr<GameSession> m_gameSession;
    FrameTimer m_frameTimer;
    bool m_isCleanedUp = false;

    VkImage m_viewportImage = VK_NULL_HANDLE;
    VkDeviceMemory m_viewportImageMemory = VK_NULL_HANDLE;
    VkImageView m_viewportImageView = VK_NULL_HANDLE;
    VkSampler m_viewportSampler = VK_NULL_HANDLE;
    VkDescriptorSet m_viewportDescriptorSet = VK_NULL_HANDLE;
    VkCommandBuffer m_viewportCommandBuffer = VK_NULL_HANDLE;
    VkRenderPass m_viewportRenderPass = VK_NULL_HANDLE;
    VkFramebuffer m_viewportFramebuffer = VK_NULL_HANDLE;
    VkExtent2D m_viewportExtent{};
    bool m_viewportResourcesCreated = false;
    bool m_viewportImageReady = false;
    float m_viewportAnimation = 0.0f;

    int m_selectedItem = -1;
    bool m_showHierarchy = true;
    bool m_showInspector = true;
    bool m_showContentBrowser = true;
    bool m_showViewport = true;

    const uint32_t WIDTH = 1280;
    const uint32_t HEIGHT = 720;
};
