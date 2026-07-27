#include "VulkanRenderer.h"

#include "Core/Log.h"
#include "Renderer/VulkanContext.h"

VulkanRenderer::VulkanRenderer() = default;

VulkanRenderer::~VulkanRenderer() {
    cleanup();
}

void VulkanRenderer::init() {
    m_frameHost.init({ "Horizon Unseen", WIDTH, HEIGHT, false });
    m_window = m_frameHost.getWindow();
    m_context = &m_frameHost.getContext();

    m_sceneRenderer.init(*m_context, "assets");

    HU_LOG_INFO("Renderer", "Window %ux%u, %u frames in flight",
                WIDTH, HEIGHT, VulkanFrameHost::framesInFlight());
}

void VulkanRenderer::renderFrame() {
    m_frameHost.renderFrame(*this);
}

VkClearValue VulkanRenderer::getClearColor() const {
    // Near-black with a faint blue cast; the starfield supplies the rest.
    return {{{ 0.016f, 0.020f, 0.043f, 1.0f }}};
}

void VulkanRenderer::recordScene(VkCommandBuffer commandBuffer) {
    if (!m_drawList || m_drawList->empty()) {
        return;
    }

    m_sceneRenderer.record(commandBuffer,
                           *m_drawList,
                           m_frameHost.getCurrentFrameIndex(),
                           getViewportWidth(),
                           getViewportHeight());
}

void VulkanRenderer::renderUi() {
    if (m_uiCallback) {
        m_uiCallback();
    }
}

bool VulkanRenderer::shouldClose() const {
    return m_frameHost.shouldClose();
}

void VulkanRenderer::waitIdle() {
    m_frameHost.waitIdle();
}

void VulkanRenderer::cleanup() {
    if (m_isCleanedUp) {
        return;
    }

    if (m_context) {
        m_context->waitIdle();
    }

    // Scene resources must go before the context/frame host that own the device.
    m_sceneRenderer.cleanup();
    m_frameHost.cleanup();

    m_context = nullptr;
    m_window = nullptr;
    m_isCleanedUp = true;
}
