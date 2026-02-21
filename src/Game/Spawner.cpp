#include "Spawner.h"

Spawner::Spawner(float x, float y, float spawnInterval)
    : m_position{x, y}
    , m_spawnTimer(0.0f)
    , m_spawnInterval(spawnInterval)
    , m_enemyType(EnemyType::Basic)
    , m_enabled(true)
{
}

void Spawner::update(float deltaTime, SpawnCallback onSpawn) {
    if (!m_enabled) return;
    
    m_spawnTimer += deltaTime;
    
    if (m_spawnTimer >= m_spawnInterval) {
        m_spawnTimer = 0.0f;
        
        // Create and spawn enemy
        auto enemy = createEnemy();
        if (enemy && onSpawn) {
            onSpawn(std::move(enemy));
        }
    }
}

std::unique_ptr<Enemy> Spawner::createEnemy() {
    switch (m_enemyType) {
        case EnemyType::Basic:
            return std::make_unique<Enemy>(m_position.x, m_position.y);
        
        // Add more enemy types here:
        // case EnemyType::Bomber:
        //     return std::make_unique<BomberEnemy>(m_position.x, m_position.y);
        
        default:
            return std::make_unique<Enemy>(m_position.x, m_position.y);
    }
}
