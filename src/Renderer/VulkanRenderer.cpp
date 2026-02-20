#include "VulkanRenderer.h"
#include "VulkanContext.h"
#include "Game/GameScene.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <stdexcept>

VulkanRenderer::VulkanRenderer() : m_window(nullptr) {}

VulkanRenderer::~VulkanRenderer() {
    cleanup();
}

void VulkanRenderer::init() {
    initWindow();
    initVulkan();
    createVertexBuffer();
    createCommandBuffers();
    createSyncObjects();
    initImGui();
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

    // TODO: Initialize ImGui Vulkan backend with proper descriptors
    // ImGui_ImplGlfw_InitForVulkan(m_window, true);
    // ImGui_ImplVulkan_Init(...);
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

    // Bind vertex buffer
    VkBuffer vertexBuffers[] = {m_vertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

    // Set push constants for player position
    if (m_gameScene) {
        struct PushConstants {
            float posX, posY;
            float scaleX, scaleY;
        } pushConstants;

        pushConstants.posX = m_gameScene->getPlayer().getPosition().x;
        pushConstants.posY = m_gameScene->getPlayer().getPosition().y;
        pushConstants.scaleX = 1.0f;
        pushConstants.scaleY = 1.0f;

        vkCmdPushConstants(commandBuffer, m_context->getPipelineLayout(), 
                          VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &pushConstants);
    }

    // Draw triangle (3 vertices)
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);

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
    // TODO: Render ImGui
    // ImGui::Begin("Debug");
    // ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
    // ImGui::End();
    // ImGui::Render();
    // ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
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

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroySemaphore(m_context->getDevice(), m_renderFinishedSemaphores[i], nullptr);
            vkDestroySemaphore(m_context->getDevice(), m_imageAvailableSemaphores[i], nullptr);
            vkDestroyFence(m_context->getDevice(), m_inFlightFences[i], nullptr);
        }
    }

    if (m_context) {
        m_context->cleanup();
    }

    // ImGui_ImplVulkan_Shutdown();
    // ImGui_ImplGlfw_Shutdown();
    // ImGui::DestroyContext();

    if (m_window) {
        glfwDestroyWindow(m_window);
        glfwTerminate();
    }

    m_isCleanedUp = true;
}
