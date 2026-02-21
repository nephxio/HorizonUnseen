#pragma once

#include "Entity.h"
#include "Enemy.h"
#include <memory>
#include <functional>

enum class EnemyType {
    Basic
    // Add more enemy types here as we create them
    // Bomber,
    // FastScout,
    // etc.
};

class Spawner {
public:
    using SpawnCallback = std::function<void(std::unique_ptr<Enemy>)>;

    Spawner(float x, float y, float spawnInterval = 3.0f);
    
    void update(float deltaTime, SpawnCallback onSpawn);
    
    void setSpawnInterval(float interval) { m_spawnInterval = interval; }
    void setEnemyType(EnemyType type) { m_enemyType = type; }
    void setEnabled(bool enabled) { m_enabled = enabled; }
    
    Vector2 getPosition() const { return m_position; }
    bool isEnabled() const { return m_enabled; }

private:
    std::unique_ptr<Enemy> createEnemy();

    Vector2 m_position;
    float m_spawnTimer;
    float m_spawnInterval;
    EnemyType m_enemyType;
    bool m_enabled;
};
