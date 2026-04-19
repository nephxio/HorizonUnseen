#include "EnemyBullet.h"
#include "Config/GameConfig.h"

EnemyBullet::EnemyBullet()
    : Entity(0.0f, 0.0f, 1.0f, 0.0f)
    , m_active(false)
{
}

void EnemyBullet::activate(float x, float y, float velocityX, float velocityY, float damage) {
    m_position.x = x;
    m_position.y = y;
    m_velocity.x = velocityX;
    m_velocity.y = velocityY;
    m_damage = damage;
    m_health = 1.0f;
    m_isAlive = true;
    m_active = true;
}

void EnemyBullet::deactivate() {
    m_active = false;
    m_isAlive = false;
}

void EnemyBullet::update(float deltaTime) {
    if (!m_active) return;
    
    m_position.x += m_velocity.x * deltaTime;
    m_position.y += m_velocity.y * deltaTime;
    
    // Deactivate if off-screen
    if (isOffScreen()) {
        deactivate();
    }
}

bool EnemyBullet::isOffScreen() const {
    auto& config = GameConfig::getInstance();
    return m_position.x > config.screenWidth + 50.0f || 
           m_position.x < -50.0f ||
           m_position.y > config.screenHeight + 50.0f || 
           m_position.y < -50.0f;
}

CollisionBox EnemyBullet::getCollisionBox() const {
    float halfWidth = 4.0f;
    float halfHeight = 4.0f;
    return CollisionBox{
        Vector2{m_position.x - halfWidth, m_position.y - halfHeight}, 
        halfWidth * 2.0f, 
        halfHeight * 2.0f
    };
}

void EnemyBullet::onCollision(ICollidable* other) {
    if (other) {
        // Player takes damage via collision handling elsewhere; bullet just deactivates.
    }
    deactivate();
}
