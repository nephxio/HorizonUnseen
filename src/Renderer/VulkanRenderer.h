#pragma once

// Window + frame plumbing for the game executable.
//
// The renderer owns the window, the Vulkan context and the batched sprite
// renderer. It does not know what a scene is: each frame the application hands
// it a DrawList to draw and a callback that emits the UI.

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include <functional>

#include "Core/DrawList.h"
#include "Rendering/SceneRenderer.h"
#include "Rendering/VulkanFrameHost.h"

class VulkanContext;

class VulkanRenderer : public IVulkanFrameClient {
public:
    using UiCallback = std::function<void()>;

    VulkanRenderer();
    ~VulkanRenderer();

    void init();
    void cleanup();

    void renderFrame();

    // Valid only for the duration of the following renderFrame call; the
    // application owns the list.
    void setDrawList(const hu::DrawList* drawList) { m_drawList = drawList; }
    void setUiCallback(UiCallback callback) { m_uiCallback = std::move(callback); }

    VkClearValue getClearColor() const override;
    void recordScene(VkCommandBuffer commandBuffer) override;
    void renderUi() override;

    bool shouldClose() const;
    void waitIdle();
    GLFWwindow* getWindow() const { return m_window; }

    float getViewportWidth() const { return static_cast<float>(WIDTH); }
    float getViewportHeight() const { return static_cast<float>(HEIGHT); }

private:
    GLFWwindow* m_window = nullptr;
    VulkanContext* m_context = nullptr;
    VulkanFrameHost m_frameHost;
    SceneRenderer m_sceneRenderer;

    const hu::DrawList* m_drawList = nullptr;
    UiCallback m_uiCallback;

    bool m_isCleanedUp = false;

    const uint32_t WIDTH = 1280;
    const uint32_t HEIGHT = 720;
};
