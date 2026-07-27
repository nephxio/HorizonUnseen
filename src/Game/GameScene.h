#pragma once

#include "Player.h"
#include "Enemy.h"
#include "Spawner.h"
#include "Bullet.h"
#include "BulletPool.h"
#include "HealthSystem.h"
#include <memory>
#include <vector>

class InputSystem;

class GameScene {
public:
    GameScene();

    void update(float deltaTime, const InputSystem& input);

    Player& getPlayer() { return m_player; }
    const Player& getPlayer() const { return m_player; }
    std::vector<std::unique_ptr<Enemy>>& getEnemies() { return m_enemies; }
    const std::vector<std::unique_ptr<Enemy>>& getEnemies() const { return m_enemies; }
    std::vector<std::unique_ptr<Bullet>>& getBullets() { return m_bullets; }
    const std::vector<std::unique_ptr<Bullet>>& getBullets() const { return m_bullets; }
    BulletPool& getEnemyBulletPool() { return m_enemyBulletPool; }
    const BulletPool& getEnemyBulletPool() const { return m_enemyBulletPool; }
    const HealthSystem& getPlayerHealthSystem() const { return m_player.getHealthSystem(); }

    int getCollisionCount() const { return m_collisionCount; }

    void spawnBullet(float x, float y, float velocityX, float velocityY, float damage);
    void spawnEnemyBullet(float x, float y, float velocityX, float velocityY, float damage);

private:
    void initializeSpawners();
    void updateSpawners(float deltaTime);
    void updateEnemies(float deltaTime);
    void updateBullets(float deltaTime);
    void removeOffScreenEnemies();
    void removeDeadEnemies();
    void removeOffScreenBullets();
    void checkCollisions();
    void onEnemySpawned(std::unique_ptr<Enemy> enemy);

    Player m_player;
    std::vector<std::unique_ptr<Enemy>> m_enemies;
    std::vector<std::unique_ptr<Bullet>> m_bullets;
    std::vector<std::unique_ptr<Spawner>> m_spawners;

    BulletPool m_enemyBulletPool;

    int m_collisionCount = 0;
    float m_lastCollisionTime = 0.0f;
    float m_collisionCooldown = 0.5f;

    // TODO: Add these systems:
    // std::vector<std::unique_ptr<Bullet>> m_bullets;
    // std::vector<std::unique_ptr<Powerup>> m_powerups;
    // ParticleSystem m_particles;
    // float m_scrollSpeed = 50.0f;
};
