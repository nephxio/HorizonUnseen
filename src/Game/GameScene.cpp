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

    // Handle shooting - check if player can shoot and space is pressed
    if (input.isKeyPressed(GLFW_KEY_SPACE) && m_player.canShoot()) {
        auto& config = GameConfig::getInstance();
        Vector2 playerPos = m_player.getPosition();
        spawnBullet(playerPos.x + 25.0f, playerPos.y, 
                    config.playerBulletSpeed, 0.0f, 
                    config.playerBulletDamage);
        m_player.shoot();
    }

    // Update spawners
    updateSpawners(deltaTime);

    // Update enemies
    updateEnemies(deltaTime);

    // Update bullets
    updateBullets(deltaTime);

    // Update enemy bullet pool
    m_enemyBulletPool.update(deltaTime);

    // Enemy shooting logic
    for (auto& enemy : m_enemies) {
        if (enemy->isAlive() && enemy->canShoot()) {
            Vector2 enemyPos = enemy->getPosition();
            // Shoot towards player (simple straight shot for now)
            spawnEnemyBullet(enemyPos.x - 20.0f, enemyPos.y, 
                            -GameConfig::getInstance().enemyBulletSpeed, 0.0f,
                            GameConfig::getInstance().enemyBulletDamage);
            enemy->shoot();
        }
    }

    // Check collisions
    checkCollisions();

    // Remove dead enemies
    removeDeadEnemies();

    // Remove off-screen enemies
    removeOffScreenEnemies();

    // Remove off-screen bullets
    removeOffScreenBullets();

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

    // Check player vs enemies
    if (!m_enemies.empty()) {
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

    // Check bullets vs enemies
    for (auto& bullet : m_bullets) {
        if (bullet->isAlive()) {
            auto bulletBox = bullet->getCollisionBox();

            for (auto& enemy : m_enemies) {
                if (enemy->isAlive()) {
                    auto enemyBox = enemy->getCollisionBox();

                    if (CollisionSystem::checkCollision(bulletBox, enemyBox)) {
                        bullet->onCollision(enemy.get());
                        enemy->onCollision(bullet.get());
                        break;
                    }
                }
            }
        }
    }

    // Check enemy bullets vs player
    if (m_player.isAlive()) {
        auto playerBox = m_player.getCollisionBox();
        auto enemyBullets = m_enemyBulletPool.getActiveBullets();

        for (auto* enemyBullet : enemyBullets) {
            if (enemyBullet->isActive()) {
                auto bulletBox = enemyBullet->getCollisionBox();

                if (CollisionSystem::checkCollision(playerBox, bulletBox)) {
                    // Only process if cooldown elapsed
                    if (m_lastCollisionTime >= m_collisionCooldown) {
                        m_collisionCount++;
                        m_lastCollisionTime = 0.0f;

                        m_player.onCollision(enemyBullet);
                        enemyBullet->onCollision(&m_player);
                    }
                    break;
                }
            }
        }
    }
}

void GameScene::onEnemySpawned(std::unique_ptr<Enemy> enemy) {
    m_enemies.push_back(std::move(enemy));
}

void GameScene::spawnBullet(float x, float y, float velocityX, float velocityY, float damage) {
    m_bullets.push_back(std::make_unique<Bullet>(x, y, velocityX, velocityY, damage));
}

void GameScene::spawnEnemyBullet(float x, float y, float velocityX, float velocityY, float damage) {
    m_enemyBulletPool.acquire(x, y, velocityX, velocityY, damage);
}

void GameScene::updateBullets(float deltaTime) {
    for (auto& bullet : m_bullets) {
        bullet->update(deltaTime);
    }
}

void GameScene::removeOffScreenBullets() {
    m_bullets.erase(
        std::remove_if(m_bullets.begin(), m_bullets.end(),
            [](const std::unique_ptr<Bullet>& bullet) {
                return bullet->isOffScreen() || !bullet->isAlive();
            }),
        m_bullets.end()
    );
}
