#include "GameScene.h"
#include "Systems/InputSystem.h"
#include <algorithm>
#include <cstdlib>

GameScene::GameScene() : m_player() {}

void GameScene::update(float deltaTime, const InputSystem& input) {
    // Update player
    m_player.handleInput(input, deltaTime);
    m_player.update(deltaTime);

    // Spawn enemies
    m_enemySpawnTimer += deltaTime;
    if (m_enemySpawnTimer >= m_enemySpawnInterval) {
        spawnEnemy();
        m_enemySpawnTimer = 0.0f;
    }

    // Update enemies
    updateEnemies(deltaTime);

    // Remove off-screen enemies
    removeOffScreenEnemies();

    // TODO: Update bullets
    // TODO: Handle collisions
    // TODO: Update particles
    // TODO: Handle background scrolling
}

void GameScene::spawnEnemy() {
    // Spawn to the right off-screen, near the top (between y=50 and y=200)
    float spawnX = 1280.0f + 50.0f;  // Off screen to the right
    float spawnY = 50.0f + (rand() % 150);  // Random position near top

    m_enemies.push_back(std::make_unique<Enemy>(spawnX, spawnY));
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
