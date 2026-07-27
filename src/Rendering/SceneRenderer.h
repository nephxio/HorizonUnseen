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

class GameScene;
class VulkanContext;

class SceneRenderer {
public:
    SceneRenderer() = default;
    ~SceneRenderer();

    SceneRenderer(const SceneRenderer&) = delete;
    SceneRenderer& operator=(const SceneRenderer&) = delete;

    void init(VulkanContext& context, const std::string& assetDirectory = "assets");

    // Primary path. The draw list is expected to be sorted by layer already;
    // that order is preserved exactly.
    void record(VkCommandBuffer commandBuffer, const hu::DrawList& drawList,
                std::uint32_t frameIndex, float viewportWidth, float viewportHeight);

    // Compatibility shim for callers that still hand over a GameScene. Builds a
    // draw list from the scene and forwards to the call above. Slated for
    // removal once gameplay emits its own draw list.
    void record(VkCommandBuffer commandBuffer, const GameScene& scene);

    void cleanup();

    const SpriteBatch& getSpriteBatch() const { return m_spriteBatch; }

private:
    void buildDrawListFromScene(const GameScene& scene);
    Vector2 spriteSizeOrDefault(hu::SpriteId id, float fallbackWidth, float fallbackHeight) const;

    VulkanContext* m_context = nullptr;
    SpriteBatch m_spriteBatch;
    bool m_isInitialized = false;

    // Scratch list reused by the compatibility path so the steady state does not
    // allocate.
    hu::DrawList m_sceneDrawList;

    // The compatibility overload gets no frame index from its caller, so the
    // renderer rotates its own to keep instance regions separated.
    std::uint32_t m_compatFrameIndex = 0;
};
