#pragma once

#include "EnemyBullet.h"
#include <vector>
#include <memory>

class BulletPool {
public:
    BulletPool(size_t poolSize = 100);
    
    // Acquire a bullet from the pool (returns nullptr if all in use)
    EnemyBullet* acquire(float x, float y, float velocityX, float velocityY, float damage);
    
    // Update all active bullets
    void update(float deltaTime);
    
    // Get active bullet count
    size_t getActiveCount() const;

    // Get pool capacity
    size_t getCapacity() const { return m_pool.size(); }

    // Get all active bullets for rendering/collision
    std::vector<EnemyBullet*> getActiveBullets() const;

    // Resize pool (clears all bullets)
    void resize(size_t newSize);

private:
    std::vector<std::unique_ptr<EnemyBullet>> m_pool;
    size_t m_nextIndex = 0;
};
