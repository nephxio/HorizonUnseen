#pragma once

// Draws a hu::DrawList with the instanced sprite batcher.
//
// SceneRenderer is deliberately thin: it owns a SpriteBatch, forwards the draw
// list to it, and knows the asset directory. All Vulkan resource handling lives
// in SpriteBatch, Texture and SpriteAtlas.

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include "Core/DrawList.h"
#include "Rendering/SpriteBatch.h"

#include <cstdint>
#include <string>

class VulkanContext;

class SceneRenderer {
public:
    SceneRenderer() = default;
    ~SceneRenderer();

    SceneRenderer(const SceneRenderer&) = delete;
    SceneRenderer& operator=(const SceneRenderer&) = delete;

    void init(VulkanContext& context, const std::string& assetDirectory = "assets");

    // The draw list is expected to be sorted by layer already; that order is
    // preserved exactly.
    void record(VkCommandBuffer commandBuffer, const hu::DrawList& drawList,
                std::uint32_t frameIndex, float viewportWidth, float viewportHeight);

    void cleanup();

    const SpriteBatch& getSpriteBatch() const { return m_spriteBatch; }

private:
    VulkanContext* m_context = nullptr;
    SpriteBatch m_spriteBatch;
    bool m_isInitialized = false;
};
