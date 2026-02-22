#include "GameScene.h"
#include "Systems/InputSystem.h"
#include "Config/GameConfig.h"
#include <algorithm>
#include <iostream>
#include <fstream>

GameScene::GameScene() : m_player() {
    initializeSpawners();
}

void GameScene::update(float deltaTime, const InputSystem& input) {
    m_lastCollisionTime += deltaTime;

    // Update player
    m_player.handleInput(input, deltaTime);
    m_player.update(deltaTime);

    // Update spawners
    updateSpawners(deltaTime);

    // Update enemies
    updateEnemies(deltaTime);

    // Check collisions
    checkCollisions();

    // Remove dead enemies
    removeDeadEnemies();

    // Remove off-screen enemies
    removeOffScreenEnemies();

    // TODO: Update bullets
    // TODO: Update particles
    // TODO: Handle background scrolling
}

void GameScene::initializeSpawners() {
    // Create 5 spawners positioned evenly across the vertical space
    // Screen height is 720, divide into 6 sections (5 spawners in the middle)
    const float screenHeight = 720.0f;
    const float spawnX = 1330.0f; // Off-screen to the right
    const int numSpawners = 5;
    const float sectionHeight = screenHeight / (numSpawners + 1);

    for (int i = 0; i < numSpawners; ++i) {
        float spawnY = sectionHeight * (i + 1);

        // Create spawner with staggered intervals for variety
        float spawnInterval = 2.5f + (i * 0.5f); // 2.5s, 3.0s, 3.5s, 4.0s, 4.5s

        auto spawner = std::make_unique<Spawner>(spawnX, spawnY, spawnInterval);
        spawner->setEnemyType(EnemyType::Basic);
        m_spawners.push_back(std::move(spawner));
    }
}

void GameScene::updateSpawners(float deltaTime) {
    for (auto& spawner : m_spawners) {
        spawner->update(deltaTime, [this](std::unique_ptr<Enemy> enemy) {
            onEnemySpawned(std::move(enemy));
        });
    }
}

void GameScene::updateEnemies(float deltaTime) {
    for (auto& enemy : m_enemies) {
        enemy->update(deltaTime);
    }
}

void GameScene::removeOffScreenEnemies() {
    m_enemies.erase(
        std::remove_if(m_enemies.begin(), m_enemies.end(),
            [](const std::unique_ptr<Enemy>& enemy) {
                return enemy->isOffScreen();
            }),
        m_enemies.end()
    );
}

void GameScene::removeDeadEnemies() {
    m_enemies.erase(
        std::remove_if(m_enemies.begin(), m_enemies.end(),
            [](const std::unique_ptr<Enemy>& enemy) {
                return !enemy->isAlive();
            }),
        m_enemies.end()
    );
}

void GameScene::checkCollisions() {
    if (!m_player.isAlive()) return;
    if (m_enemies.empty()) return;

    // Check player vs enemies
    auto playerBox = m_player.getCollisionBox();

    for (size_t i = 0; i < m_enemies.size(); ++i) {
        auto& enemy = m_enemies[i];
        if (enemy->isAlive()) {
            auto enemyBox = enemy->getCollisionBox();

            if (CollisionSystem::checkCollision(playerBox, enemyBox)) {
                // Only process collision if cooldown has elapsed
                if (m_lastCollisionTime >= m_collisionCooldown) {
                    m_collisionCount++;
                    m_lastCollisionTime = 0.0f;  // Reset cooldown

                    m_player.onCollision(enemy.get());
                    enemy->onCollision(&m_player);
                }

                // Exit after first collision to prevent multiple collisions per frame
                return;
            }
        }
    }
}

void GameScene::onEnemySpawned(std::unique_ptr<Enemy> enemy) {
    m_enemies.push_back(std::move(enemy));
}
