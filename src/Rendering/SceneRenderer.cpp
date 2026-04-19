#include "SceneRenderer.h"

#include "Game/GameScene.h"
#include "Renderer/VulkanContext.h"

#include <cstring>
#include <stdexcept>
#include <vector>

SceneRenderer::~SceneRenderer() {
    cleanup();
}

void SceneRenderer::init(VulkanContext& context) {
    m_context = &context;

    createVertexBuffer();
    createEnemyVertexBuffer();
    createBulletVertexBuffer();
    createEnemyBulletVertexBuffer();

    m_isInitialized = true;
}

void SceneRenderer::createVertexBuffer() {
    std::vector<float> vertices = {
        -20.0f,  20.0f,  1.0f, 0.0f, 0.0f,
        -20.0f, -20.0f,  0.0f, 1.0f, 0.0f,
         20.0f,   0.0f,  0.0f, 0.0f, 1.0f
    };

    VkDeviceSize bufferSize = sizeof(float) * vertices.size();

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = bufferSize;
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(m_context->getDevice(), &bufferInfo, nullptr, &m_vertexBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create vertex buffer");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(m_context->getDevice(), m_vertexBuffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;

    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(m_context->getPhysicalDevice(), &memProperties);

    uint32_t memoryTypeIndex = 0;
    VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((memRequirements.memoryTypeBits & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            memoryTypeIndex = i;
            break;
        }
    }
    allocInfo.memoryTypeIndex = memoryTypeIndex;

    if (vkAllocateMemory(m_context->getDevice(), &allocInfo, nullptr, &m_vertexBufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate vertex buffer memory");
    }

    vkBindBufferMemory(m_context->getDevice(), m_vertexBuffer, m_vertexBufferMemory, 0);

    void* data;
    vkMapMemory(m_context->getDevice(), m_vertexBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, vertices.data(), static_cast<size_t>(bufferSize));
    vkUnmapMemory(m_context->getDevice(), m_vertexBufferMemory);
}

void SceneRenderer::createEnemyVertexBuffer() {
    std::vector<float> vertices = {
        -15.0f, -15.0f,  1.0f, 0.0f, 0.0f,
         15.0f, -15.0f,  1.0f, 0.0f, 0.0f,
         15.0f,  15.0f,  1.0f, 0.0f, 0.0f,

         15.0f,  15.0f,  1.0f, 0.0f, 0.0f,
        -15.0f,  15.0f,  1.0f, 0.0f, 0.0f,
        -15.0f, -15.0f,  1.0f, 0.0f, 0.0f
    };

    VkDeviceSize bufferSize = sizeof(float) * vertices.size();

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = bufferSize;
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(m_context->getDevice(), &bufferInfo, nullptr, &m_enemyVertexBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create enemy vertex buffer");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(m_context->getDevice(), m_enemyVertexBuffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;

    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(m_context->getPhysicalDevice(), &memProperties);

    uint32_t memoryTypeIndex = 0;
    VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((memRequirements.memoryTypeBits & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            memoryTypeIndex = i;
            break;
        }
    }
    allocInfo.memoryTypeIndex = memoryTypeIndex;

    if (vkAllocateMemory(m_context->getDevice(), &allocInfo, nullptr, &m_enemyVertexBufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate enemy vertex buffer memory");
    }

    vkBindBufferMemory(m_context->getDevice(), m_enemyVertexBuffer, m_enemyVertexBufferMemory, 0);

    void* data;
    vkMapMemory(m_context->getDevice(), m_enemyVertexBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, vertices.data(), static_cast<size_t>(bufferSize));
    vkUnmapMemory(m_context->getDevice(), m_enemyVertexBufferMemory);
}

void SceneRenderer::createBulletVertexBuffer() {
    std::vector<float> vertices = {
        -3.0f, -3.0f,  1.0f, 1.0f, 0.0f,
         3.0f, -3.0f,  1.0f, 1.0f, 0.0f,
         3.0f,  3.0f,  1.0f, 1.0f, 0.0f,

         3.0f,  3.0f,  1.0f, 1.0f, 0.0f,
        -3.0f,  3.0f,  1.0f, 1.0f, 0.0f,
        -3.0f, -3.0f,  1.0f, 1.0f, 0.0f
    };

    VkDeviceSize bufferSize = sizeof(float) * vertices.size();

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = bufferSize;
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(m_context->getDevice(), &bufferInfo, nullptr, &m_bulletVertexBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create bullet vertex buffer");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(m_context->getDevice(), m_bulletVertexBuffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;

    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(m_context->getPhysicalDevice(), &memProperties);

    uint32_t memoryTypeIndex = 0;
    VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((memRequirements.memoryTypeBits & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            memoryTypeIndex = i;
            break;
        }
    }
    allocInfo.memoryTypeIndex = memoryTypeIndex;

    if (vkAllocateMemory(m_context->getDevice(), &allocInfo, nullptr, &m_bulletVertexBufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate bullet vertex buffer memory");
    }

    vkBindBufferMemory(m_context->getDevice(), m_bulletVertexBuffer, m_bulletVertexBufferMemory, 0);

    void* data;
    vkMapMemory(m_context->getDevice(), m_bulletVertexBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, vertices.data(), static_cast<size_t>(bufferSize));
    vkUnmapMemory(m_context->getDevice(), m_bulletVertexBufferMemory);
}

void SceneRenderer::createEnemyBulletVertexBuffer() {
    std::vector<float> vertices = {
        -4.0f, -4.0f,  1.0f, 0.5f, 0.0f,
         4.0f, -4.0f,  1.0f, 0.5f, 0.0f,
         4.0f,  4.0f,  1.0f, 0.5f, 0.0f,

         4.0f,  4.0f,  1.0f, 0.5f, 0.0f,
        -4.0f,  4.0f,  1.0f, 0.5f, 0.0f,
        -4.0f, -4.0f,  1.0f, 0.5f, 0.0f
    };

    VkDeviceSize bufferSize = sizeof(float) * vertices.size();

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = bufferSize;
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(m_context->getDevice(), &bufferInfo, nullptr, &m_enemyBulletVertexBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create enemy bullet vertex buffer");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(m_context->getDevice(), m_enemyBulletVertexBuffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;

    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(m_context->getPhysicalDevice(), &memProperties);

    uint32_t memoryTypeIndex = 0;
    VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((memRequirements.memoryTypeBits & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            memoryTypeIndex = i;
            break;
        }
    }
    allocInfo.memoryTypeIndex = memoryTypeIndex;

    if (vkAllocateMemory(m_context->getDevice(), &allocInfo, nullptr, &m_enemyBulletVertexBufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate enemy bullet vertex buffer memory");
    }

    vkBindBufferMemory(m_context->getDevice(), m_enemyBulletVertexBuffer, m_enemyBulletVertexBufferMemory, 0);

    void* data;
    vkMapMemory(m_context->getDevice(), m_enemyBulletVertexBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, vertices.data(), static_cast<size_t>(bufferSize));
    vkUnmapMemory(m_context->getDevice(), m_enemyBulletVertexBufferMemory);
}

void SceneRenderer::record(VkCommandBuffer commandBuffer, const GameScene& scene) const {
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_context->getPipeline());

    struct PushConstants {
        float posX, posY;
        float scaleX, scaleY;
    } pushConstants;

    VkDeviceSize offsets[] = { 0 };

    VkBuffer vertexBuffers[] = { m_vertexBuffer };
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    pushConstants.posX = scene.getPlayer().getPosition().x;
    pushConstants.posY = scene.getPlayer().getPosition().y;
    pushConstants.scaleX = 1.0f;
    pushConstants.scaleY = 1.0f;
    vkCmdPushConstants(commandBuffer, m_context->getPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &pushConstants);
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);

    VkBuffer enemyVertexBuffers[] = { m_enemyVertexBuffer };
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, enemyVertexBuffers, offsets);
    for (const auto& enemy : scene.getEnemies()) {
        pushConstants.posX = enemy->getPosition().x;
        pushConstants.posY = enemy->getPosition().y;
        pushConstants.scaleX = 1.0f;
        pushConstants.scaleY = 1.0f;
        vkCmdPushConstants(commandBuffer, m_context->getPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &pushConstants);
        vkCmdDraw(commandBuffer, 6, 1, 0, 0);
    }

    VkBuffer bulletVertexBuffers[] = { m_bulletVertexBuffer };
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, bulletVertexBuffers, offsets);
    for (const auto& bullet : scene.getBullets()) {
        pushConstants.posX = bullet->getPosition().x;
        pushConstants.posY = bullet->getPosition().y;
        pushConstants.scaleX = 1.0f;
        pushConstants.scaleY = 1.0f;
        vkCmdPushConstants(commandBuffer, m_context->getPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &pushConstants);
        vkCmdDraw(commandBuffer, 6, 1, 0, 0);
    }

    VkBuffer enemyBulletVertexBuffers[] = { m_enemyBulletVertexBuffer };
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, enemyBulletVertexBuffers, offsets);
    auto enemyBullets = scene.getEnemyBulletPool().getActiveBullets();
    for (const auto* enemyBullet : enemyBullets) {
        pushConstants.posX = enemyBullet->getPosition().x;
        pushConstants.posY = enemyBullet->getPosition().y;
        pushConstants.scaleX = 1.0f;
        pushConstants.scaleY = 1.0f;
        vkCmdPushConstants(commandBuffer, m_context->getPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &pushConstants);
        vkCmdDraw(commandBuffer, 6, 1, 0, 0);
    }
}

void SceneRenderer::cleanup() {
    if (!m_context || !m_isInitialized) {
        return;
    }

    VkDevice device = m_context->getDevice();
    if (m_vertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, m_vertexBuffer, nullptr);
    }
    if (m_vertexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_vertexBufferMemory, nullptr);
    }
    if (m_enemyVertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, m_enemyVertexBuffer, nullptr);
    }
    if (m_enemyVertexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_enemyVertexBufferMemory, nullptr);
    }
    if (m_bulletVertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, m_bulletVertexBuffer, nullptr);
    }
    if (m_bulletVertexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_bulletVertexBufferMemory, nullptr);
    }
    if (m_enemyBulletVertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, m_enemyBulletVertexBuffer, nullptr);
    }
    if (m_enemyBulletVertexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_enemyBulletVertexBufferMemory, nullptr);
    }

    m_vertexBuffer = VK_NULL_HANDLE;
    m_vertexBufferMemory = VK_NULL_HANDLE;
    m_enemyVertexBuffer = VK_NULL_HANDLE;
    m_enemyVertexBufferMemory = VK_NULL_HANDLE;
    m_bulletVertexBuffer = VK_NULL_HANDLE;
    m_bulletVertexBufferMemory = VK_NULL_HANDLE;
    m_enemyBulletVertexBuffer = VK_NULL_HANDLE;
    m_enemyBulletVertexBufferMemory = VK_NULL_HANDLE;
    m_isInitialized = false;
    m_context = nullptr;
}
