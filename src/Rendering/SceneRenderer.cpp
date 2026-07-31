#include "SceneRenderer.h"

#include "Core/Log.h"
#include "Renderer/VulkanContext.h"

#include <stdexcept>

namespace {

constexpr const char* kLogCategory = "SceneRenderer";

} // namespace

SceneRenderer::~SceneRenderer() {
    cleanup();
}

void SceneRenderer::init(VulkanContext& context, const std::string& assetDirectory) {
    m_context = &context;

    if (!m_spriteBatch.init(context, assetDirectory)) {
        throw std::runtime_error("Failed to initialise the sprite batch");
    }

    m_isInitialized = true;
}

void SceneRenderer::record(VkCommandBuffer commandBuffer, const hu::DrawList& drawList,
                           std::uint32_t frameIndex, float viewportWidth, float viewportHeight) {
    if (!m_isInitialized) {
        return;
    }

    m_spriteBatch.record(commandBuffer, drawList, frameIndex, viewportWidth, viewportHeight);
}

void SceneRenderer::cleanup() {
    if (!m_isInitialized) {
        m_context = nullptr;
        return;
    }

    m_spriteBatch.cleanup();
    m_isInitialized = false;
    m_context = nullptr;

    HU_LOG_INFO(kLogCategory, "Scene renderer shut down");
}
