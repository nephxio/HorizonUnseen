#include "BulletPool.h"

BulletPool::BulletPool(size_t poolSize) {
    m_pool.reserve(poolSize);
    for (size_t i = 0; i < poolSize; ++i) {
        m_pool.push_back(std::make_unique<EnemyBullet>());
    }
}

EnemyBullet* BulletPool::acquire(float x, float y, float velocityX, float velocityY, float damage) {
    // Search for an inactive bullet starting from m_nextIndex
    size_t startIndex = m_nextIndex;
    
    do {
        if (!m_pool[m_nextIndex]->isActive()) {
            EnemyBullet* bullet = m_pool[m_nextIndex].get();
            bullet->activate(x, y, velocityX, velocityY, damage);
            
            // Move to next index for next allocation
            m_nextIndex = (m_nextIndex + 1) % m_pool.size();
            return bullet;
        }
        
        m_nextIndex = (m_nextIndex + 1) % m_pool.size();
    } while (m_nextIndex != startIndex);
    
    // Pool is full - all bullets in use
    return nullptr;
}

void BulletPool::update(float deltaTime) {
    for (auto& bullet : m_pool) {
        if (bullet->isActive()) {
            bullet->update(deltaTime);
        }
    }
}

size_t BulletPool::getActiveCount() const {
    size_t count = 0;
    for (const auto& bullet : m_pool) {
        if (bullet->isActive()) {
            ++count;
        }
    }
    return count;
}

std::vector<EnemyBullet*> BulletPool::getActiveBullets() const {
    std::vector<EnemyBullet*> activeBullets;
    activeBullets.reserve(m_pool.size());

    for (const auto& bullet : m_pool) {
        if (bullet->isActive()) {
            activeBullets.push_back(bullet.get());
        }
    }

    return activeBullets;
}

void BulletPool::resize(size_t newSize) {
    m_pool.clear();
    m_pool.reserve(newSize);
    for (size_t i = 0; i < newSize; ++i) {
        m_pool.push_back(std::make_unique<EnemyBullet>());
    }
    m_nextIndex = 0;
}
