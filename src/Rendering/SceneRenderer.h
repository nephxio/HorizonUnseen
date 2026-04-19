#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include <memory>

class GameScene;
class VulkanContext;

class SceneRenderer {
public:
    SceneRenderer() = default;
    ~SceneRenderer();

    void init(VulkanContext& context);
    void record(VkCommandBuffer commandBuffer, const GameScene& scene) const;
    void cleanup();

private:
    void createVertexBuffer();
    void createEnemyVertexBuffer();
    void createBulletVertexBuffer();
    void createEnemyBulletVertexBuffer();

    VulkanContext* m_context = nullptr;
    bool m_isInitialized = false;

    VkBuffer m_vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_vertexBufferMemory = VK_NULL_HANDLE;

    VkBuffer m_enemyVertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_enemyVertexBufferMemory = VK_NULL_HANDLE;

    VkBuffer m_bulletVertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_bulletVertexBufferMemory = VK_NULL_HANDLE;

    VkBuffer m_enemyBulletVertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_enemyBulletVertexBufferMemory = VK_NULL_HANDLE;
};
