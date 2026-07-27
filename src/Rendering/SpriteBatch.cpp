#include "SpriteBatch.h"

#include "Core/DrawList.h"
#include "Core/Log.h"
#include "Renderer/VulkanContext.h"

#include <algorithm>
#include <cstring>
#include <fstream>

namespace {

constexpr const char* kLogCategory = "SpriteBatch";

// Unit quad spanning -0.5..0.5, two triangles, no index buffer. Six vertices of
// eight bytes is cheap enough that indexing would be a false economy.
constexpr float kQuadVertices[] = {
    -0.5f, -0.5f,
     0.5f, -0.5f,
     0.5f,  0.5f,

     0.5f,  0.5f,
    -0.5f,  0.5f,
    -0.5f, -0.5f
};

struct SpritePushConstants {
    float viewportWidth;
    float viewportHeight;
    float padding0;
    float padding1;
};

std::string withTrailingSlash(std::string path) {
    if (!path.empty() && path.back() != '/' && path.back() != '\\') {
        path.push_back('/');
    }
    return path;
}

// The working directory differs between running from the build tree, the source
// tree and an installed layout, so walk a few levels up looking for the atlas
// rather than demanding one exact cwd.
std::string resolveAssetDirectory(const std::string& requested) {
    const std::string base = withTrailingSlash(requested);

    std::string prefix;
    for (int depth = 0; depth < 4; ++depth) {
        const std::string candidate = prefix + base;
        std::ifstream probe(candidate + "atlas.json", std::ios::binary);
        if (probe.is_open()) {
            return candidate;
        }
        prefix += "../";
    }

    return base;
}

} // namespace

SpriteBatch::~SpriteBatch() {
    cleanup();
}

bool SpriteBatch::init(VulkanContext& context, const std::string& assetDirectory) {
    m_context = &context;

    const std::string directory = resolveAssetDirectory(assetDirectory);
    const std::string atlasJsonPath = directory + "atlas.json";

    bool atlasLoaded = m_atlas.loadFromFile(atlasJsonPath);
    if (atlasLoaded) {
        const std::string imageName = m_atlas.getImageName().empty() ? std::string("atlas.png")
                                                                     : m_atlas.getImageName();
        const std::string imagePath = directory + imageName;

        if (!m_atlasTexture.loadFromFile(context, imagePath)) {
            HU_LOG_ERROR(kLogCategory, "Atlas image '%s' could not be loaded; falling back to a white texel",
                         imagePath.c_str());
            atlasLoaded = false;
        } else {
            HU_LOG_INFO(kLogCategory, "Atlas loaded: '%s' (%ux%u), %zu/%zu sprites resolved, capacity %u instances",
                        imagePath.c_str(), m_atlasTexture.getWidth(), m_atlasTexture.getHeight(),
                        m_atlas.getResolvedCount(), hu::SpriteIdCount, kMaxInstances);
        }
    } else {
        HU_LOG_ERROR(kLogCategory, "Atlas description '%s' could not be loaded", atlasJsonPath.c_str());
    }

    if (!atlasLoaded) {
        // Untextured fallback: every sprite samples a single white texel, so the
        // game still renders correctly-shaped tinted quads.
        m_atlas.setIdentityMapping();
        m_atlasTexture.cleanup();
        if (!m_atlasTexture.createWhite(context)) {
            HU_LOG_ERROR(kLogCategory, "Failed to create the fallback white texture; sprite rendering is disabled");
            return false;
        }
        HU_LOG_WARN(kLogCategory, "Rendering with a 1x1 white fallback texture, capacity %u instances", kMaxInstances);
    }

    if (!createQuadBuffer() || !createInstanceBuffer() || !createDescriptorSet()) {
        cleanup();
        return false;
    }

    m_isInitialized = true;
    return true;
}

bool SpriteBatch::allocateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                                 VkBuffer& buffer, VkDeviceMemory& memory) {
    VkDevice device = m_context->getDevice();

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        HU_LOG_ERROR(kLogCategory, "Failed to create a %llu byte buffer",
                     static_cast<unsigned long long>(size));
        return false;
    }

    VkMemoryRequirements requirements;
    vkGetBufferMemoryRequirements(device, buffer, &requirements);

    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(m_context->getPhysicalDevice(), &memProperties);

    std::uint32_t memoryTypeIndex = 0;
    bool found = false;
    for (std::uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
        if ((requirements.memoryTypeBits & (1u << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            memoryTypeIndex = i;
            found = true;
            break;
        }
    }

    if (!found) {
        HU_LOG_ERROR(kLogCategory, "No memory type satisfies the requested buffer properties");
        vkDestroyBuffer(device, buffer, nullptr);
        buffer = VK_NULL_HANDLE;
        return false;
    }

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = requirements.size;
    allocInfo.memoryTypeIndex = memoryTypeIndex;

    if (vkAllocateMemory(device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        HU_LOG_ERROR(kLogCategory, "Failed to allocate %llu bytes of buffer memory",
                     static_cast<unsigned long long>(requirements.size));
        vkDestroyBuffer(device, buffer, nullptr);
        buffer = VK_NULL_HANDLE;
        return false;
    }

    vkBindBufferMemory(device, buffer, memory, 0);
    return true;
}

bool SpriteBatch::createQuadBuffer() {
    const VkDeviceSize size = sizeof(kQuadVertices);

    if (!allocateBuffer(size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        m_quadBuffer, m_quadMemory)) {
        return false;
    }

    void* mapped = nullptr;
    vkMapMemory(m_context->getDevice(), m_quadMemory, 0, size, 0, &mapped);
    std::memcpy(mapped, kQuadVertices, static_cast<std::size_t>(size));
    vkUnmapMemory(m_context->getDevice(), m_quadMemory);
    return true;
}

bool SpriteBatch::createInstanceBuffer() {
    // One contiguous allocation carved into kFramesInFlight equal regions. The
    // buffer stays mapped for the lifetime of the batcher; re-mapping every
    // frame would be pure overhead.
    const VkDeviceSize regionSize = static_cast<VkDeviceSize>(kMaxInstances) * sizeof(GpuInstance);
    const VkDeviceSize totalSize = regionSize * kFramesInFlight;

    if (!allocateBuffer(totalSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        m_instanceBuffer, m_instanceMemory)) {
        return false;
    }

    void* mapped = nullptr;
    if (vkMapMemory(m_context->getDevice(), m_instanceMemory, 0, totalSize, 0, &mapped) != VK_SUCCESS) {
        HU_LOG_ERROR(kLogCategory, "Failed to map the instance buffer");
        return false;
    }

    m_mappedInstances = static_cast<GpuInstance*>(mapped);
    return true;
}

bool SpriteBatch::createDescriptorSet() {
    VkDevice device = m_context->getDevice();

    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 1;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
        HU_LOG_ERROR(kLogCategory, "Failed to create the sprite descriptor pool");
        return false;
    }

    VkDescriptorSetLayout layout = m_context->getSpriteDescriptorSetLayout();
    if (layout == VK_NULL_HANDLE) {
        HU_LOG_ERROR(kLogCategory, "VulkanContext has no sprite descriptor set layout");
        return false;
    }

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    if (vkAllocateDescriptorSets(device, &allocInfo, &m_descriptorSet) != VK_SUCCESS) {
        HU_LOG_ERROR(kLogCategory, "Failed to allocate the sprite descriptor set");
        return false;
    }

    // The atlas never changes after init, so the set is written exactly once.
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = m_atlasTexture.getImageView();
    imageInfo.sampler = m_atlasTexture.getSampler();

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_descriptorSet;
    write.dstBinding = 0;
    write.dstArrayElement = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.descriptorCount = 1;
    write.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
    return true;
}

void SpriteBatch::record(VkCommandBuffer commandBuffer, const hu::DrawList& drawList,
                         std::uint32_t frameIndex, float viewportWidth, float viewportHeight) {
    m_lastFrameStats = FrameStats{};

    if (!m_isInitialized || m_mappedInstances == nullptr) {
        return;
    }

    const std::vector<hu::SpriteInstance>& sprites = drawList.sprites();
    if (sprites.empty()) {
        return;
    }

    const std::uint32_t region = frameIndex % kFramesInFlight;
    const std::uint32_t regionBase = region * kMaxInstances;
    GpuInstance* target = m_mappedInstances + regionBase;

    std::uint32_t count = static_cast<std::uint32_t>(std::min<std::size_t>(sprites.size(), kMaxInstances));
    if (sprites.size() > kMaxInstances) {
        m_lastFrameStats.droppedInstances = static_cast<std::uint32_t>(sprites.size() - kMaxInstances);
        if (!m_loggedOverflow) {
            m_loggedOverflow = true;
            HU_LOG_WARN(kLogCategory, "DrawList of %zu exceeds the %u instance capacity; %u sprites dropped",
                        sprites.size(), kMaxInstances, m_lastFrameStats.droppedInstances);
        }
    }

    for (std::uint32_t i = 0; i < count; ++i) {
        const hu::SpriteInstance& source = sprites[i];
        const hu::UvRect& uv = m_atlas.getUv(source.sprite);

        GpuInstance& instance = target[i];
        instance.centerX = source.position.x;
        instance.centerY = source.position.y;
        instance.sizeX = source.size.x;
        instance.sizeY = source.size.y;
        instance.rotation = source.rotation;
        instance.r = source.color.r;
        instance.g = source.color.g;
        instance.b = source.color.b;
        instance.a = source.color.a;
        instance.u0 = uv.u0;
        instance.v0 = uv.v0;
        instance.u1 = uv.u1;
        instance.v1 = uv.v1;
    }

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = viewportWidth;
    viewport.height = viewportHeight;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent.width = static_cast<std::uint32_t>(std::max(viewportWidth, 0.0f));
    scissor.extent.height = static_cast<std::uint32_t>(std::max(viewportHeight, 0.0f));
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    const VkDeviceSize instanceRegionOffset =
        static_cast<VkDeviceSize>(regionBase) * sizeof(GpuInstance);

    VkBuffer buffers[] = { m_quadBuffer, m_instanceBuffer };
    VkDeviceSize offsets[] = { 0, instanceRegionOffset };
    vkCmdBindVertexBuffers(commandBuffer, 0, 2, buffers, offsets);

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_context->getPipelineLayout(), 0, 1, &m_descriptorSet, 0, nullptr);

    SpritePushConstants pushConstants{};
    pushConstants.viewportWidth = viewportWidth;
    pushConstants.viewportHeight = viewportHeight;
    vkCmdPushConstants(commandBuffer, m_context->getPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0,
                       sizeof(SpritePushConstants), &pushConstants);

    // Walk the list in submission order and emit one draw per run of sprites
    // sharing a blend mode. Because instance data is laid out in that same
    // order, each run is a contiguous [first, first + length) slice and needs no
    // rebinding -- only a pipeline switch.
    VkPipeline alphaPipeline = m_context->getSpriteAlphaPipeline();
    VkPipeline additivePipeline = m_context->getSpriteAdditivePipeline();

    VkPipeline boundPipeline = VK_NULL_HANDLE;
    std::uint32_t runStart = 0;
    bool runAdditive = sprites[0].additive;
    std::uint32_t batches = 0;

    for (std::uint32_t i = 1; i <= count; ++i) {
        const bool endOfRun = (i == count) || (sprites[i].additive != runAdditive);
        if (!endOfRun) {
            continue;
        }

        VkPipeline wanted = runAdditive ? additivePipeline : alphaPipeline;
        if (wanted != boundPipeline) {
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, wanted);
            boundPipeline = wanted;
        }

        vkCmdDraw(commandBuffer, 6, i - runStart, 0, runStart);
        ++batches;

        if (i < count) {
            runStart = i;
            runAdditive = sprites[i].additive;
        }
    }

    m_lastFrameStats.instanceCount = count;
    m_lastFrameStats.batchCount = batches;

    HU_LOG_TRACE(kLogCategory, "frame %u: %u instances in %u batches", frameIndex, count, batches);
}

void SpriteBatch::cleanup() {
    if (m_context == nullptr) {
        return;
    }

    VkDevice device = m_context->getDevice();
    if (device == VK_NULL_HANDLE) {
        m_context = nullptr;
        m_isInitialized = false;
        return;
    }

    if (m_descriptorPool != VK_NULL_HANDLE) {
        // Frees m_descriptorSet along with the pool.
        vkDestroyDescriptorPool(device, m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
        m_descriptorSet = VK_NULL_HANDLE;
    }

    if (m_mappedInstances != nullptr) {
        vkUnmapMemory(device, m_instanceMemory);
        m_mappedInstances = nullptr;
    }
    if (m_instanceBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, m_instanceBuffer, nullptr);
        m_instanceBuffer = VK_NULL_HANDLE;
    }
    if (m_instanceMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_instanceMemory, nullptr);
        m_instanceMemory = VK_NULL_HANDLE;
    }

    if (m_quadBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, m_quadBuffer, nullptr);
        m_quadBuffer = VK_NULL_HANDLE;
    }
    if (m_quadMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_quadMemory, nullptr);
        m_quadMemory = VK_NULL_HANDLE;
    }

    m_atlasTexture.cleanup();

    m_isInitialized = false;
    m_context = nullptr;
}
