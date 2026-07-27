#include "SceneRenderer.h"

#include "Core/Log.h"
#include "Game/GameScene.h"
#include "Renderer/VulkanContext.h"

#include <stdexcept>

namespace {

constexpr const char* kLogCategory = "SceneRenderer";

// World extent assumed by the compatibility path, matching the default window.
constexpr float kDefaultWorldWidth = 1280.0f;
constexpr float kDefaultWorldHeight = 720.0f;

} // namespace

SceneRenderer::~SceneRenderer() {
    cleanup();
}

void SceneRenderer::init(VulkanContext& context, const std::string& assetDirectory) {
    m_context = &context;

    if (!m_spriteBatch.init(context, assetDirectory)) {
        throw std::runtime_error("Failed to initialise the sprite batch");
    }

    m_sceneDrawList.reserve(1024);
    m_isInitialized = true;
}

void SceneRenderer::record(VkCommandBuffer commandBuffer, const hu::DrawList& drawList,
                           std::uint32_t frameIndex, float viewportWidth, float viewportHeight) {
    if (!m_isInitialized) {
        return;
    }

    m_spriteBatch.record(commandBuffer, drawList, frameIndex, viewportWidth, viewportHeight);
}

Vector2 SceneRenderer::spriteSizeOrDefault(hu::SpriteId id, float fallbackWidth, float fallbackHeight) const {
    const hu::AtlasRect& rect = m_spriteBatch.getAtlas().getSourceRect(id);
    if (rect.w > 0 && rect.h > 0) {
        return Vector2{ static_cast<float>(rect.w), static_cast<float>(rect.h) };
    }
    return Vector2{ fallbackWidth, fallbackHeight };
}

void SceneRenderer::buildDrawListFromScene(const GameScene& scene) {
    m_sceneDrawList.clear();

    const Vector2 playerSize = spriteSizeOrDefault(hu::SpriteId::ShipPlayer, 40.0f, 40.0f);
    m_sceneDrawList.add(hu::SpriteId::ShipPlayer, scene.getPlayer().getPosition(), playerSize,
                        hu::DrawLayer::Player);

    const Vector2 enemySize = spriteSizeOrDefault(hu::SpriteId::EnemyDrifter, 30.0f, 30.0f);
    for (const auto& enemy : scene.getEnemies()) {
        if (enemy == nullptr) {
            continue;
        }
        m_sceneDrawList.add(hu::SpriteId::EnemyDrifter, enemy->getPosition(), enemySize,
                            hu::DrawLayer::Enemy);
    }

    const Vector2 bulletSize = spriteSizeOrDefault(hu::SpriteId::BulletPlayer, 6.0f, 6.0f);
    for (const auto& bullet : scene.getBullets()) {
        if (bullet == nullptr) {
            continue;
        }
        m_sceneDrawList.add(hu::SpriteId::BulletPlayer, bullet->getPosition(), bulletSize,
                            hu::DrawLayer::PlayerProjectile, hu::White, 0.0f, /*additive=*/true);
    }

    const Vector2 enemyBulletSize = spriteSizeOrDefault(hu::SpriteId::BulletEnemy, 8.0f, 8.0f);
    for (const auto* enemyBullet : scene.getEnemyBulletPool().getActiveBullets()) {
        if (enemyBullet == nullptr) {
            continue;
        }
        m_sceneDrawList.add(hu::SpriteId::BulletEnemy, enemyBullet->getPosition(), enemyBulletSize,
                            hu::DrawLayer::EnemyProjectile, hu::White, 0.0f, /*additive=*/true);
    }

    m_sceneDrawList.sortByLayer();
}

void SceneRenderer::record(VkCommandBuffer commandBuffer, const GameScene& scene) {
    if (!m_isInitialized) {
        return;
    }

    buildDrawListFromScene(scene);

    const std::uint32_t frameIndex = m_compatFrameIndex;
    m_compatFrameIndex = (m_compatFrameIndex + 1) % SpriteBatch::kFramesInFlight;

    m_spriteBatch.record(commandBuffer, m_sceneDrawList, frameIndex,
                         kDefaultWorldWidth, kDefaultWorldHeight);
}

void SceneRenderer::cleanup() {
    if (!m_isInitialized) {
        m_context = nullptr;
        return;
    }

    m_spriteBatch.cleanup();
    m_sceneDrawList.clear();
    m_isInitialized = false;
    m_context = nullptr;

    HU_LOG_INFO(kLogCategory, "Scene renderer shut down");
}
