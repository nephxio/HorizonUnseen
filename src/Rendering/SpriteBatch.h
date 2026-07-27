#pragma once

// Instanced sprite batcher.
//
// Owns everything the sprite pipelines need to draw a hu::DrawList: the shared
// unit quad, the atlas texture and its descriptor set, and a persistently
// mapped instance ring buffer with one region per frame in flight.
//
// Draw order is sacred. The DrawList arrives already sorted by layer and this
// class never reorders it; batches are split whenever the blend mode changes so
// that a particle drawn after a ship still lands on top of it. Fewer draw calls
// is not worth a wrong-looking frame.

#include "Rendering/SpriteAtlas.h"
#include "Rendering/Texture.h"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <string>

namespace hu {
class DrawList;
}

class VulkanContext;

class SpriteBatch {
public:
    // Sized for a dense bullet-hell frame with room to spare.
    static constexpr std::uint32_t kMaxInstances = 20000;

    // Must match VulkanFrameHost::MAX_FRAMES_IN_FLIGHT: each in-flight frame
    // writes its own slice of the instance buffer so the GPU is never reading a
    // region the CPU is overwriting.
    static constexpr std::uint32_t kFramesInFlight = 2;

    struct FrameStats {
        std::uint32_t instanceCount = 0;
        std::uint32_t batchCount = 0;
        std::uint32_t droppedInstances = 0;
    };

    SpriteBatch() = default;
    ~SpriteBatch();

    SpriteBatch(const SpriteBatch&) = delete;
    SpriteBatch& operator=(const SpriteBatch&) = delete;

    // Loads <assetDirectory>/atlas.json and its image. Always succeeds in the
    // sense that rendering stays possible: a failed atlas load falls back to a
    // generated 1x1 white texture and identity UVs.
    bool init(VulkanContext& context, const std::string& assetDirectory);

    void record(VkCommandBuffer commandBuffer, const hu::DrawList& drawList,
                std::uint32_t frameIndex, float viewportWidth, float viewportHeight);

    void cleanup();

    bool isInitialized() const { return m_isInitialized; }
    const hu::SpriteAtlas& getAtlas() const { return m_atlas; }
    const FrameStats& getLastFrameStats() const { return m_lastFrameStats; }

private:
    // Mirrors the per-instance vertex binding declared in VulkanContext.
    struct GpuInstance {
        float centerX, centerY;
        float sizeX, sizeY;
        float rotation;
        float r, g, b, a;
        float u0, v0, u1, v1;
    };
    static_assert(sizeof(GpuInstance) == sizeof(float) * 13, "Instance layout must match the pipeline binding");

    bool createQuadBuffer();
    bool createInstanceBuffer();
    bool createDescriptorSet();
    bool allocateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                        VkBuffer& buffer, VkDeviceMemory& memory);

    VulkanContext* m_context = nullptr;
    bool m_isInitialized = false;

    hu::SpriteAtlas m_atlas;
    Texture m_atlasTexture;

    VkBuffer m_quadBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_quadMemory = VK_NULL_HANDLE;

    VkBuffer m_instanceBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_instanceMemory = VK_NULL_HANDLE;
    GpuInstance* m_mappedInstances = nullptr;

    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;

    FrameStats m_lastFrameStats{};
    bool m_loggedOverflow = false;
};
