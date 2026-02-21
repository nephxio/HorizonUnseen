#pragma once

#include "Player.h"
#include "Enemy.h"
#include <memory>
#include <vector>

class InputSystem;

class GameScene {
public:
    GameScene();

    void update(float deltaTime, const InputSystem& input);

    const Player& getPlayer() const { return m_player; }
    const std::vector<std::unique_ptr<Enemy>>& getEnemies() const { return m_enemies; }

private:
    void spawnEnemy();
    void updateEnemies(float deltaTime);
    void removeOffScreenEnemies();

    Player m_player;
    std::vector<std::unique_ptr<Enemy>> m_enemies;

    float m_enemySpawnTimer = 0.0f;
    float m_enemySpawnInterval = 2.0f;

    // TODO: Add these systems:
    // std::vector<std::unique_ptr<Bullet>> m_bullets;
    // std::vector<std::unique_ptr<Powerup>> m_powerups;
    // ParticleSystem m_particles;
    // float m_scrollSpeed = 50.0f;
};
