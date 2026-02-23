#include "VulkanRenderer.h"
#include "VulkanContext.h"
#include "Game/GameScene.h"
#include "Debug/DebugConsole.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <stdexcept>
#include <iostream>

VulkanRenderer::VulkanRenderer() : m_window(nullptr) {}

VulkanRenderer::~VulkanRenderer() {
    cleanup();
}

void VulkanRenderer::init() {
    std::cout << " - initWindow()" << std::endl;
    initWindow();
    std::cout << " - initVulkan()" << std::endl;
    initVulkan();
    std::cout << " - createVertexBuffer()" << std::endl;
    createVertexBuffer();
    std::cout << " - createEnemyVertexBuffer()" << std::endl;
    createEnemyVertexBuffer();
    std::cout << " - createBulletVertexBuffer()" << std::endl;
    createBulletVertexBuffer();
    std::cout << " - createCommandBuffers()" << std::endl;
    createCommandBuffers();
    std::cout << " - createSyncObjects()" << std::endl;
    createSyncObjects();
    std::cout << " - initImGui()" << std::endl;
    initImGui();
    std::cout << " - Renderer init complete!" << std::endl;
}

void VulkanRenderer::initWindow() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    m_window = glfwCreateWindow(WIDTH, HEIGHT, "Horizon Unseen", nullptr, nullptr);
    if (!m_window) {
        throw std::runtime_error("Failed to create GLFW window");
    }
}

void VulkanRenderer::initVulkan() {
    m_context = std::make_unique<VulkanContext>();
    m_context->init(m_window);
}

void VulkanRenderer::initImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForVulkan(m_window, true);

    // Create descriptor pool for ImGui
    VkDescriptorPoolSize poolSizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 1000;
    poolInfo.poolSizeCount = sizeof(poolSizes) / sizeof(poolSizes[0]);
    poolInfo.pPoolSizes = poolSizes;

    if (vkCreateDescriptorPool(m_context->getDevice(), &poolInfo, nullptr, &m_imguiDescriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create ImGui descriptor pool");
    }

    ImGui_ImplVulkan_InitInfo initInfo = {};
    initInfo.ApiVersion = VK_API_VERSION_1_2;
    initInfo.Instance = m_context->getInstance();
    initInfo.PhysicalDevice = m_context->getPhysicalDevice();
    initInfo.Device = m_context->getDevice();
    initInfo.QueueFamily = 0;
    initInfo.Queue = m_context->getGraphicsQueue();
    initInfo.PipelineCache = VK_NULL_HANDLE;
    initInfo.DescriptorPool = m_imguiDescriptorPool;
    initInfo.MinImageCount = MAX_FRAMES_IN_FLIGHT;
    initInfo.ImageCount = MAX_FRAMES_IN_FLIGHT;
    initInfo.Allocator = nullptr;
    initInfo.CheckVkResultFn = nullptr;

    // Set pipeline info for the main viewport
    initInfo.PipelineInfoMain.RenderPass = m_context->getRenderPass();
    initInfo.PipelineInfoMain.Subpass = 0;
    initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&initInfo);
}

void VulkanRenderer::createCommandBuffers() {
    m_commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_context->getCommandPool();
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(m_commandBuffers.size());

    if (vkAllocateCommandBuffers(m_context->getDevice(), &allocInfo, m_commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffers");
    }
}

void VulkanRenderer::createSyncObjects() {
    m_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(m_context->getDevice(), &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(m_context->getDevice(), &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(m_context->getDevice(), &fenceInfo, nullptr, &m_inFlightFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create synchronization objects");
        }
    }
}

void VulkanRenderer::createVertexBuffer() {
    // Triangle vertices: position (x, y) + color (r, g, b)
    // Triangle pointing right (like a spaceship)
    std::vector<float> vertices = {
        // Position      // Color
        -20.0f,  20.0f,  1.0f, 0.0f, 0.0f,  // Top-left (red)
        -20.0f, -20.0f,  0.0f, 1.0f, 0.0f,  // Bottom-left (green)
         20.0f,   0.0f,  0.0f, 0.0f, 1.0f   // Right point (blue)
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

    // Find memory type
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

    // Copy vertex data
    void* data;
    vkMapMemory(m_context->getDevice(), m_vertexBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, vertices.data(), (size_t)bufferSize);
    vkUnmapMemory(m_context->getDevice(), m_vertexBufferMemory);
}

void VulkanRenderer::createEnemyVertexBuffer() {
    // Square vertices: position (x, y) + color (r, g, b)
    // Two triangles forming a square centered at origin
    std::vector<float> vertices = {
        // First triangle
        -15.0f, -15.0f,  1.0f, 0.0f, 0.0f,  // Bottom-left (red)
         15.0f, -15.0f,  1.0f, 0.0f, 0.0f,  // Bottom-right (red)
         15.0f,  15.0f,  1.0f, 0.0f, 0.0f,  // Top-right (red)

        // Second triangle
         15.0f,  15.0f,  1.0f, 0.0f, 0.0f,  // Top-right (red)
        -15.0f,  15.0f,  1.0f, 0.0f, 0.0f,  // Top-left (red)
        -15.0f, -15.0f,  1.0f, 0.0f, 0.0f   // Bottom-left (red)
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

    // Find memory type
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

    // Copy vertex data
    void* data;
    vkMapMemory(m_context->getDevice(), m_enemyVertexBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, vertices.data(), (size_t)bufferSize);
    vkUnmapMemory(m_context->getDevice(), m_enemyVertexBufferMemory);
}

void VulkanRenderer::createBulletVertexBuffer() {
    // Small square for bullets: position (x, y) + color (r, g, b)
    std::vector<float> vertices = {
        // First triangle
        -3.0f, -3.0f,  1.0f, 1.0f, 0.0f,  // Bottom-left (yellow)
         3.0f, -3.0f,  1.0f, 1.0f, 0.0f,  // Bottom-right (yellow)
         3.0f,  3.0f,  1.0f, 1.0f, 0.0f,  // Top-right (yellow)

        // Second triangle
         3.0f,  3.0f,  1.0f, 1.0f, 0.0f,  // Top-right (yellow)
        -3.0f,  3.0f,  1.0f, 1.0f, 0.0f,  // Top-left (yellow)
        -3.0f, -3.0f,  1.0f, 1.0f, 0.0f   // Bottom-left (yellow)
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

    // Find memory type
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

    // Copy vertex data
    void* data;
    vkMapMemory(m_context->getDevice(), m_bulletVertexBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, vertices.data(), (size_t)bufferSize);
    vkUnmapMemory(m_context->getDevice(), m_bulletVertexBufferMemory);
}

void VulkanRenderer::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin recording command buffer");
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_context->getRenderPass();
    renderPassInfo.framebuffer = m_context->getFramebuffers()[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = m_context->getSwapchainExtent();

    VkClearValue clearColor = {{{0.0f, 0.0f, 0.2f, 1.0f}}};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Bind the graphics pipeline
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_context->getPipeline());

    struct PushConstants {
        float posX, posY;
        float scaleX, scaleY;
    } pushConstants;

    if (m_gameScene) {
        // Draw player (triangle)
        VkBuffer vertexBuffers[] = {m_vertexBuffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

        pushConstants.posX = m_gameScene->getPlayer().getPosition().x;
        pushConstants.posY = m_gameScene->getPlayer().getPosition().y;
        pushConstants.scaleX = 1.0f;
        pushConstants.scaleY = 1.0f;

        vkCmdPushConstants(commandBuffer, m_context->getPipelineLayout(), 
                          VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &pushConstants);

        vkCmdDraw(commandBuffer, 3, 1, 0, 0);

        // Draw enemies (squares)
        VkBuffer enemyVertexBuffers[] = {m_enemyVertexBuffer};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, enemyVertexBuffers, offsets);

        for (const auto& enemy : m_gameScene->getEnemies()) {
            pushConstants.posX = enemy->getPosition().x;
            pushConstants.posY = enemy->getPosition().y;
            pushConstants.scaleX = 1.0f;
            pushConstants.scaleY = 1.0f;

            vkCmdPushConstants(commandBuffer, m_context->getPipelineLayout(), 
                              VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &pushConstants);

            vkCmdDraw(commandBuffer, 6, 1, 0, 0);
        }

        // Draw bullets (small yellow squares)
        VkBuffer bulletVertexBuffers[] = {m_bulletVertexBuffer};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, bulletVertexBuffers, offsets);

        for (const auto& bullet : m_gameScene->getBullets()) {
            pushConstants.posX = bullet->getPosition().x;
            pushConstants.posY = bullet->getPosition().y;
            pushConstants.scaleX = 1.0f;
            pushConstants.scaleY = 1.0f;

            vkCmdPushConstants(commandBuffer, m_context->getPipelineLayout(), 
                              VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &pushConstants);

            vkCmdDraw(commandBuffer, 6, 1, 0, 0);
        }
    }

    // Render ImGui only if there's draw data
    ImDrawData* drawData = ImGui::GetDrawData();
    if (drawData && drawData->TotalVtxCount > 0) {
        ImGui_ImplVulkan_RenderDrawData(drawData, commandBuffer);
    }

    vkCmdEndRenderPass(commandBuffer);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to record command buffer");
    }
}

void VulkanRenderer::beginFrame() {
    glfwPollEvents();

    vkWaitForFences(m_context->getDevice(), 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);

    VkResult result = vkAcquireNextImageKHR(m_context->getDevice(), m_context->getSwapchain(), UINT64_MAX,
        m_imageAvailableSemaphores[m_currentFrame], VK_NULL_HANDLE, &m_imageIndex);

    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Failed to acquire swap chain image");
    }

    vkResetFences(m_context->getDevice(), 1, &m_inFlightFences[m_currentFrame]);

    vkResetCommandBuffer(m_commandBuffers[m_currentFrame], 0);
    recordCommandBuffer(m_commandBuffers[m_currentFrame], m_imageIndex);
}

void VulkanRenderer::renderScene(const GameScene& scene) {
    // Scene rendering is handled in recordCommandBuffer
    // We pass the player position via push constants
}

void VulkanRenderer::renderUI() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Render HUD (always visible)
    DebugConsole::getInstance().renderHUD(m_gameScene);

    // Render debug console (only when visible)
    DebugConsole::getInstance().render();

    ImGui::Render();
}

void VulkanRenderer::endFrame() {
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {m_imageAvailableSemaphores[m_currentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_commandBuffers[m_currentFrame];

    VkSemaphore signalSemaphores[] = {m_renderFinishedSemaphores[m_currentFrame]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(m_context->getGraphicsQueue(), 1, &submitInfo, m_inFlightFences[m_currentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit draw command buffer");
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = {m_context->getSwapchain()};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &m_imageIndex;

    vkQueuePresentKHR(m_context->getPresentQueue(), &presentInfo);

    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

bool VulkanRenderer::shouldClose() const {
    return glfwWindowShouldClose(m_window);
}

void VulkanRenderer::waitIdle() {
    if (m_context) {
        m_context->waitIdle();
    }
}

void VulkanRenderer::cleanup() {
    if (m_isCleanedUp) {
        return;
    }

    if (m_context) {
        m_context->waitIdle();
    }

    if (m_context && m_context->getDevice() != VK_NULL_HANDLE) {
        if (m_vertexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_context->getDevice(), m_vertexBuffer, nullptr);
        }
        if (m_vertexBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(m_context->getDevice(), m_vertexBufferMemory, nullptr);
        }

        if (m_enemyVertexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_context->getDevice(), m_enemyVertexBuffer, nullptr);
        }
        if (m_enemyVertexBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(m_context->getDevice(), m_enemyVertexBufferMemory, nullptr);
        }

        if (m_imguiDescriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(m_context->getDevice(), m_imguiDescriptorPool, nullptr);
        }

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroySemaphore(m_context->getDevice(), m_renderFinishedSemaphores[i], nullptr);
            vkDestroySemaphore(m_context->getDevice(), m_imageAvailableSemaphores[i], nullptr);
            vkDestroyFence(m_context->getDevice(), m_inFlightFences[i], nullptr);
        }
    }

    if (m_context) {
        m_context->cleanup();
    }

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (m_window) {
        glfwDestroyWindow(m_window);
        glfwTerminate();
    }

    m_isCleanedUp = true;
}
