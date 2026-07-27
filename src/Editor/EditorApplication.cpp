#include "EditorApplication.h"

#include <imgui.h>
#include <imgui_impl_vulkan.h>

#include <algorithm>
#include <iostream>
#include <stdexcept>

#include "Game/Entity.h"

namespace {
uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
        if ((typeFilter & (1u << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type for viewport texture");
}
}

EditorApplication::EditorApplication() {
    init();
}

EditorApplication::~EditorApplication() {
    cleanup();
}

void EditorApplication::init() {
    m_frameHost.init({ "Horizon Unseen Editor", WIDTH, HEIGHT, true });
    m_gameSession = std::make_unique<GameSession>(m_frameHost.getWindow());
    m_sceneRenderer.init(m_frameHost.getContext());
    initViewportResources();
}

void EditorApplication::initViewportResources() {
    auto& context = m_frameHost.getContext();
    m_viewportExtent = context.getSwapchainExtent();

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = context.getSwapchainImageFormat();
    imageInfo.extent = { m_viewportExtent.width, m_viewportExtent.height, 1 };
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(context.getDevice(), &imageInfo, nullptr, &m_viewportImage) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create editor viewport image");
    }

    VkMemoryRequirements memoryRequirements{};
    vkGetImageMemoryRequirements(context.getDevice(), m_viewportImage, &memoryRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memoryRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(
        context.getPhysicalDevice(),
        memoryRequirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(context.getDevice(), &allocInfo, nullptr, &m_viewportImageMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate editor viewport image memory");
    }

    vkBindImageMemory(context.getDevice(), m_viewportImage, m_viewportImageMemory, 0);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_viewportImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = context.getSwapchainImageFormat();
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(context.getDevice(), &viewInfo, nullptr, &m_viewportImageView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create editor viewport image view");
    }

    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = context.getSwapchainImageFormat();
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(context.getDevice(), &renderPassInfo, nullptr, &m_viewportRenderPass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create editor viewport render pass");
    }

    VkImageView attachments[] = { m_viewportImageView };
    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = m_viewportRenderPass;
    framebufferInfo.attachmentCount = 1;
    framebufferInfo.pAttachments = attachments;
    framebufferInfo.width = m_viewportExtent.width;
    framebufferInfo.height = m_viewportExtent.height;
    framebufferInfo.layers = 1;

    if (vkCreateFramebuffer(context.getDevice(), &framebufferInfo, nullptr, &m_viewportFramebuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create editor viewport framebuffer");
    }

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;

    if (vkCreateSampler(context.getDevice(), &samplerInfo, nullptr, &m_viewportSampler) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create editor viewport sampler");
    }

    m_viewportDescriptorSet = reinterpret_cast<VkDescriptorSet>(ImGui_ImplVulkan_AddTexture(
        m_viewportSampler,
        m_viewportImageView,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));

    VkCommandBufferAllocateInfo allocCmdInfo{};
    allocCmdInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocCmdInfo.commandPool = context.getCommandPool();
    allocCmdInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocCmdInfo.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(context.getDevice(), &allocCmdInfo, &m_viewportCommandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate editor viewport command buffer");
    }

    m_viewportResourcesCreated = true;
}

void EditorApplication::run() {
    while (m_frameHost.getWindow() && !m_frameHost.shouldClose()) {
        renderFrame();
    }

    m_frameHost.waitIdle();
}

void EditorApplication::renderFrame() {
    m_frameHost.renderFrame(*this);
}

VkClearValue EditorApplication::getClearColor() const {
    return {{{0.08f, 0.08f, 0.10f, 1.0f}}};
}

void EditorApplication::recordScene(VkCommandBuffer) {
    // The viewport texture is rendered separately; this hook is unused for the editor scene pass.
}

void EditorApplication::updateViewportTexture() {
    if (!m_viewportResourcesCreated) {
        return;
    }

    auto& context = m_frameHost.getContext();
    m_gameSession->update(m_frameTimer.tick());

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkResetCommandBuffer(m_viewportCommandBuffer, 0);
    if (vkBeginCommandBuffer(m_viewportCommandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin viewport command buffer");
    }

    VkImageMemoryBarrier toColorAttachment{};
    toColorAttachment.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toColorAttachment.oldLayout = m_viewportImageReady ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
    toColorAttachment.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    toColorAttachment.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toColorAttachment.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toColorAttachment.image = m_viewportImage;
    toColorAttachment.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    toColorAttachment.subresourceRange.baseMipLevel = 0;
    toColorAttachment.subresourceRange.levelCount = 1;
    toColorAttachment.subresourceRange.baseArrayLayer = 0;
    toColorAttachment.subresourceRange.layerCount = 1;
    toColorAttachment.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    toColorAttachment.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    vkCmdPipelineBarrier(
        m_viewportCommandBuffer,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &toColorAttachment);

    VkClearValue clearColor = getClearColor();
    VkImageSubresourceRange clearRange{};
    clearRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    clearRange.baseMipLevel = 0;
    clearRange.levelCount = 1;
    clearRange.baseArrayLayer = 0;
    clearRange.layerCount = 1;

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_viewportRenderPass;
    renderPassInfo.framebuffer = m_viewportFramebuffer;
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = m_viewportExtent;
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(m_viewportCommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    m_sceneRenderer.record(m_viewportCommandBuffer, m_gameSession->getScene());

    vkCmdEndRenderPass(m_viewportCommandBuffer);

    m_viewportImageReady = true;

    if (vkEndCommandBuffer(m_viewportCommandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to end viewport command buffer");
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_viewportCommandBuffer;

    if (vkQueueSubmit(context.getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit viewport texture update");
    }

    vkQueueWaitIdle(context.getGraphicsQueue());
    m_viewportAnimation += 0.05f;
}

void EditorApplication::renderUi() {
    auto& scene = m_gameSession->getScene();
    validateSelection(scene);

    updateViewportTexture();

    renderDockspace();

    if (m_showHierarchy) renderHierarchyPanel(scene);
    if (m_showInspector) renderInspectorPanel(scene);
    if (m_showContentBrowser) renderContentBrowserPanel();
    if (m_showViewport) renderViewportPanel();
}

void EditorApplication::renderDockspace() {
    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar |
                                   ImGuiWindowFlags_NoTitleBar |
                                   ImGuiWindowFlags_NoCollapse |
                                   ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoBringToFrontOnFocus |
                                   ImGuiWindowFlags_NoNavFocus;

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);

    ImGui::Begin("EditorShell", nullptr, windowFlags);

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Exit")) {
                glfwSetWindowShouldClose(m_frameHost.getWindow(), GLFW_TRUE);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Scene Hierarchy", nullptr, &m_showHierarchy);
            ImGui::MenuItem("Inspector", nullptr, &m_showInspector);
            ImGui::MenuItem("Content Browser", nullptr, &m_showContentBrowser);
            ImGui::MenuItem("Viewport", nullptr, &m_showViewport);
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }

    ImGui::End();
}

void EditorApplication::renderHierarchyPanel(GameScene& scene) {
    ImGui::Begin("Scene Hierarchy", &m_showHierarchy);
    ImGui::Text("Runtime scene objects");
    ImGui::Separator();

    auto renderBadge = [](const char* label, const ImVec4& color) {
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextUnformatted(label);
        ImGui::PopStyleColor();
    };

    if (ImGui::TreeNodeEx("Player (1)", ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SameLine();
        renderBadge(scene.getPlayer().isAlive() ? "[Alive]" : "[Down]", scene.getPlayer().isAlive() ? ImVec4(0.35f, 0.85f, 0.45f, 1.0f) : ImVec4(0.90f, 0.35f, 0.35f, 1.0f));

        if (ImGui::Selectable("Player", isSelected(&scene.getPlayer()), ImGuiSelectableFlags_SpanAllColumns)) {
            selectEntity(SelectionKind::Player, &scene.getPlayer(), ImGui::GetIO().KeyCtrl);
        }
        ImGui::TreePop();
    }

    auto& enemies = scene.getEnemies();
    if (ImGui::TreeNodeEx("Enemies", ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_DefaultOpen, "Enemies (%d)", static_cast<int>(enemies.size()))) {
        ImGui::SameLine();
        renderBadge(enemies.empty() ? "[Empty]" : "[Active]", enemies.empty() ? ImVec4(0.70f, 0.70f, 0.70f, 1.0f) : ImVec4(0.30f, 0.65f, 0.95f, 1.0f));

        for (std::size_t i = 0; i < enemies.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            const bool selected = isSelected(enemies[i].get());
            if (ImGui::Selectable("Enemy", selected, ImGuiSelectableFlags_SpanAllColumns)) {
                selectEntity(SelectionKind::Enemy, enemies[i].get(), ImGui::GetIO().KeyCtrl);
            }
            ImGui::PopID();
        }
        ImGui::TreePop();
    }

    auto& bullets = scene.getBullets();
    if (ImGui::TreeNodeEx("Bullets", ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_DefaultOpen, "Bullets (%d)", static_cast<int>(bullets.size()))) {
        ImGui::SameLine();
        renderBadge(bullets.empty() ? "[Empty]" : "[Active]", bullets.empty() ? ImVec4(0.70f, 0.70f, 0.70f, 1.0f) : ImVec4(0.95f, 0.80f, 0.25f, 1.0f));

        for (std::size_t i = 0; i < bullets.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            const bool selected = isSelected(bullets[i].get());
            if (ImGui::Selectable("Bullet", selected, ImGuiSelectableFlags_SpanAllColumns)) {
                selectEntity(SelectionKind::Bullet, bullets[i].get(), ImGui::GetIO().KeyCtrl);
            }
            ImGui::PopID();
        }
        ImGui::TreePop();
    }

    auto activeBullets = scene.getEnemyBulletPool().getActiveBullets();
    if (ImGui::TreeNodeEx("EnemyBullets", ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_DefaultOpen, "Enemy Bullets (%d)", static_cast<int>(activeBullets.size()))) {
        ImGui::SameLine();
        renderBadge(activeBullets.empty() ? "[Empty]" : "[Active]", activeBullets.empty() ? ImVec4(0.70f, 0.70f, 0.70f, 1.0f) : ImVec4(0.95f, 0.55f, 0.20f, 1.0f));

        for (std::size_t i = 0; i < activeBullets.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            const bool selected = isSelected(activeBullets[i]);
            if (ImGui::Selectable("Enemy Bullet", selected, ImGuiSelectableFlags_SpanAllColumns)) {
                selectEntity(SelectionKind::EnemyBullet, activeBullets[i], ImGui::GetIO().KeyCtrl);
            }
            ImGui::PopID();
        }
        ImGui::TreePop();
    }

    ImGui::End();
}

void EditorApplication::renderInspectorPanel(GameScene& scene) {
    ImGui::Begin("Inspector", &m_showInspector);

    if (m_selection.empty()) {
        ImGui::TextUnformatted("Select an entity in the hierarchy.");
        ImGui::End();
        return;
    }

    if (m_selection.size() == 1) {
        auto& selected = m_selection.front();
        renderEntityInspector(*selected.entity, selected.kind);
    } else {
        ImGui::Text("Multiple selection (%zu)", m_selection.size());
        ImGui::Separator();

        if (ImGui::TreeNodeEx("Shared Properties", ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_DefaultOpen)) {
            Entity* first = m_selection.front().entity;

            Vector2 position = first->getPosition();
            if (ImGui::InputFloat2("Position", &position.x)) {
                for (auto& selected : m_selection) {
                    selected.entity->setPosition(position);
                }
            }

            Vector2 velocity = first->getVelocity();
            if (ImGui::InputFloat2("Velocity", &velocity.x)) {
                for (auto& selected : m_selection) {
                    selected.entity->setVelocity(velocity);
                }
            }

            float health = first->getHealth();
            if (ImGui::DragFloat("Health", &health, 1.0f, 0.0f, 100000.0f)) {
                for (auto& selected : m_selection) {
                    selected.entity->setHealth(health);
                }
            }

            float maxHealth = first->getMaxHealth();
            if (ImGui::DragFloat("Max Health", &maxHealth, 1.0f, 0.0f, 100000.0f)) {
                for (auto& selected : m_selection) {
                    selected.entity->setMaxHealth(maxHealth);
                }
            }

            float damage = first->getDamage();
            if (ImGui::DragFloat("Damage", &damage, 0.5f, 0.0f, 100000.0f)) {
                for (auto& selected : m_selection) {
                    selected.entity->setDamage(damage);
                }
            }

            ImGui::Text("Selected Entities: %zu", m_selection.size());
            ImGui::TreePop();
        }

        ImGui::Separator();

        for (std::size_t i = 0; i < m_selection.size(); ++i) {
            auto& selected = m_selection[i];
            ImGui::PushID(static_cast<int>(i));
            if (ImGui::TreeNodeEx(getSelectionLabel(selected.kind), ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_DefaultOpen)) {
                renderEntityInspector(*selected.entity, selected.kind);
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    }

    ImGui::End();
}

void EditorApplication::renderContentBrowserPanel() {
    ImGui::Begin("Content Browser", &m_showContentBrowser);
    ImGui::Text("Assets will appear here later.");
    ImGui::Separator();
    ImGui::BulletText("Scenes");
    ImGui::BulletText("Textures");
    ImGui::BulletText("Audio");
    ImGui::BulletText("Scripts");
    ImGui::End();
}

void EditorApplication::selectEntity(SelectionKind kind, Entity* entity, bool additive) {
    if (!additive) {
        m_selection.clear();
    }

    if (!entity) {
        return;
    }

    auto existing = std::find_if(m_selection.begin(), m_selection.end(), [entity](const SceneSelection& selection) {
        return selection.entity == entity;
    });

    if (existing != m_selection.end()) {
        if (additive) {
            m_selection.erase(existing);
        }
        return;
    }

    m_selection.push_back(SceneSelection{ kind, entity });
}

void EditorApplication::clearSelection() {
    m_selection.clear();
}

bool EditorApplication::isSelected(const Entity* entity) const {
    return std::any_of(m_selection.begin(), m_selection.end(), [entity](const SceneSelection& selection) {
        return selection.entity == entity;
    });
}

const char* EditorApplication::getSelectionLabel(SelectionKind kind) const {
    switch (kind) {
    case SelectionKind::Player: return "Player";
    case SelectionKind::Enemy: return "Enemy";
    case SelectionKind::Bullet: return "Bullet";
    case SelectionKind::EnemyBullet: return "Enemy Bullet";
    default: return "Entity";
    }
}

void EditorApplication::renderEntityInspector(Entity& entity, SelectionKind kind) {
    ImGui::Text("Entity: %p", static_cast<void*>(&entity));
    ImGui::Separator();

    Vector2 position = entity.getPosition();
    if (ImGui::InputFloat2("Position", &position.x)) {
        entity.setPosition(position);
    }

    Vector2 velocity = entity.getVelocity();
    if (ImGui::InputFloat2("Velocity", &velocity.x)) {
        entity.setVelocity(velocity);
    }

    float health = entity.getHealth();
    if (ImGui::DragFloat("Health", &health, 1.0f, 0.0f, 100000.0f)) {
        entity.setHealth(health);
    }

    float maxHealth = entity.getMaxHealth();
    if (ImGui::DragFloat("Max Health", &maxHealth, 1.0f, 0.0f, 100000.0f)) {
        entity.setMaxHealth(maxHealth);
    }

    float damage = entity.getDamage();
    if (ImGui::DragFloat("Damage", &damage, 0.5f, 0.0f, 100000.0f)) {
        entity.setDamage(damage);
    }

    ImGui::Text("Alive: %s", entity.isAlive() ? "Yes" : "No");

    if (kind == SelectionKind::Player) {
        auto& player = static_cast<Player&>(entity);
        ImGui::Separator();
        ImGui::TextUnformatted("Player Details");
        ImGui::Text("Shoot Cooldown: %.2f", player.getShootCooldown());

        HealthSystem& healthSystem = player.getHealthSystem();
        if (ImGui::TreeNode("Health System")) {
            ImGui::Text("Alive Cells: %zu / %zu", healthSystem.getAliveCellCount(), HealthSystem::CellCount);
            ImGui::Text("Total Health: %.1f / %.1f", healthSystem.getTotalHealth(), healthSystem.getTotalMaxHealth());
            ImGui::Text("Total Energy: %.1f / %.1f", healthSystem.getTotalEnergy(), healthSystem.getTotalMaxEnergy());

            auto& cells = healthSystem.getCells();
            for (std::size_t i = 0; i < cells.size(); ++i) {
                ImGui::PushID(static_cast<int>(i));
                if (ImGui::TreeNode("Cell", "Cell %zu", i)) {
                    auto& cell = cells[i];
                    ImGui::DragFloat("Current Health", &cell.currentHealth, 1.0f, 0.0f, cell.maxHealth);
                    ImGui::DragFloat("Max Health", &cell.maxHealth, 1.0f, 0.0f, 100000.0f);
                    ImGui::DragFloat("Current Energy", &cell.currentEnergy, 1.0f, 0.0f, cell.maxEnergy);
                    ImGui::DragFloat("Max Energy", &cell.maxEnergy, 1.0f, 0.0f, 100000.0f);
                    ImGui::DragFloat("Damage Threshold", &cell.damageThreshold, 0.5f, 0.0f, 100000.0f);
                    ImGui::Checkbox("Broken", &cell.broken);
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }

            ImGui::TreePop();
        }
    } else if (kind == SelectionKind::Enemy) {
        auto& enemy = static_cast<Enemy&>(entity);
        ImGui::Separator();
        ImGui::TextUnformatted("Enemy Details");
        ImGui::Text("State: %s", enemy.getState() == EnemyState::MovingLeft ? "MovingLeft" : "Diving");
        ImGui::Text("Shoot Cooldown: %.2f", enemy.getShootCooldown());
        ImGui::Text("Off Screen: %s", enemy.isOffScreen() ? "Yes" : "No");
    } else if (kind == SelectionKind::Bullet) {
        ImGui::Separator();
        ImGui::TextUnformatted("Bullet Details");
        ImGui::Text("Off Screen: %s", static_cast<Bullet&>(entity).isOffScreen() ? "Yes" : "No");
    } else if (kind == SelectionKind::EnemyBullet) {
        auto& bullet = static_cast<EnemyBullet&>(entity);
        ImGui::Separator();
        ImGui::TextUnformatted("Enemy Bullet Details");
        ImGui::Text("Active: %s", bullet.isActive() ? "Yes" : "No");
        ImGui::Text("Off Screen: %s", bullet.isOffScreen() ? "Yes" : "No");
    }
}

void EditorApplication::validateSelection(GameScene& scene) {
    if (m_selection.empty()) {
        return;
    }

    auto isEnemyBulletAlive = [&scene](const Entity* entity) {
        auto activeBullets = scene.getEnemyBulletPool().getActiveBullets();
        return std::any_of(activeBullets.begin(), activeBullets.end(), [entity](const EnemyBullet* bullet) {
            return bullet == entity;
        });
    };

    m_selection.erase(std::remove_if(m_selection.begin(), m_selection.end(), [&](const SceneSelection& selection) {
        switch (selection.kind) {
        case SelectionKind::Player:
            return selection.entity != &scene.getPlayer();
        case SelectionKind::Enemy:
            return std::none_of(scene.getEnemies().begin(), scene.getEnemies().end(), [&](const std::unique_ptr<Enemy>& enemy) {
                return enemy.get() == selection.entity;
            });
        case SelectionKind::Bullet:
            return std::none_of(scene.getBullets().begin(), scene.getBullets().end(), [&](const std::unique_ptr<Bullet>& bullet) {
                return bullet.get() == selection.entity;
            });
        case SelectionKind::EnemyBullet:
            return !isEnemyBulletAlive(selection.entity);
        default:
            return true;
        }
    }), m_selection.end());
}

void EditorApplication::renderViewportPanel() {
    ImGui::Begin("Viewport", &m_showViewport);
    ImVec2 available = ImGui::GetContentRegionAvail();
    float aspect = static_cast<float>(m_viewportExtent.width) / static_cast<float>(m_viewportExtent.height);

    ImVec2 imageSize = available;
    if (available.x > 0.0f && available.y > 0.0f) {
        if ((available.x / available.y) > aspect) {
            imageSize.y = available.y;
            imageSize.x = imageSize.y * aspect;
        } else {
            imageSize.x = available.x;
            imageSize.y = imageSize.x / aspect;
        }
    }

    if (m_viewportDescriptorSet != VK_NULL_HANDLE) {
        ImGui::Image(reinterpret_cast<ImTextureID>(m_viewportDescriptorSet), imageSize);
    } else {
        ImGui::TextUnformatted("Viewport texture is not available.");
    }

    ImGui::End();
}

void EditorApplication::cleanupViewportResources() {
    auto& context = m_frameHost.getContext();

    if (m_viewportResourcesCreated) {
        if (m_viewportDescriptorSet != VK_NULL_HANDLE) {
            ImGui_ImplVulkan_RemoveTexture(m_viewportDescriptorSet);
            m_viewportDescriptorSet = VK_NULL_HANDLE;
        }

        if (m_viewportCommandBuffer != VK_NULL_HANDLE) {
            vkFreeCommandBuffers(context.getDevice(), context.getCommandPool(), 1, &m_viewportCommandBuffer);
            m_viewportCommandBuffer = VK_NULL_HANDLE;
        }

        if (m_viewportFramebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(context.getDevice(), m_viewportFramebuffer, nullptr);
            m_viewportFramebuffer = VK_NULL_HANDLE;
        }

        if (m_viewportRenderPass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(context.getDevice(), m_viewportRenderPass, nullptr);
            m_viewportRenderPass = VK_NULL_HANDLE;
        }

        if (m_viewportSampler != VK_NULL_HANDLE) {
            vkDestroySampler(context.getDevice(), m_viewportSampler, nullptr);
            m_viewportSampler = VK_NULL_HANDLE;
        }

        if (m_viewportImageView != VK_NULL_HANDLE) {
            vkDestroyImageView(context.getDevice(), m_viewportImageView, nullptr);
            m_viewportImageView = VK_NULL_HANDLE;
        }

        if (m_viewportImage != VK_NULL_HANDLE) {
            vkDestroyImage(context.getDevice(), m_viewportImage, nullptr);
            m_viewportImage = VK_NULL_HANDLE;
        }

        if (m_viewportImageMemory != VK_NULL_HANDLE) {
            vkFreeMemory(context.getDevice(), m_viewportImageMemory, nullptr);
            m_viewportImageMemory = VK_NULL_HANDLE;
        }

        m_viewportResourcesCreated = false;
    }
}

void EditorApplication::cleanup() {
    if (m_isCleanedUp) {
        return;
    }

    cleanupViewportResources();
    m_sceneRenderer.cleanup();
    m_frameHost.cleanup();
    m_isCleanedUp = true;
}
