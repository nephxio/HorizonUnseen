#include "Bullet.h"
#include "Config/GameConfig.h"

Bullet::Bullet(float x, float y, float velocityX, float velocityY, float damage)
    : Entity(x, y, 1.0f, damage)
{
    m_velocity.x = velocityX;
    m_velocity.y = velocityY;
}

void Bullet::update(float deltaTime) {
    m_position.x += m_velocity.x * deltaTime;
    m_position.y += m_velocity.y * deltaTime;
}

bool Bullet::isOffScreen() const {
    auto& config = GameConfig::getInstance();
    return m_position.x > config.screenWidth + 50.0f || 
           m_position.x < -50.0f ||
           m_position.y > config.screenHeight + 50.0f || 
           m_position.y < -50.0f;
}

CollisionBox Bullet::getCollisionBox() const {
    float halfWidth = 3.0f;
    float halfHeight = 3.0f;
    return CollisionBox{
        Vector2{m_position.x - halfWidth, m_position.y - halfHeight}, 
        halfWidth * 2.0f, 
        halfHeight * 2.0f
    };
}

void Bullet::onCollision(ICollidable* other) {
    // Bullet dies on collision
    m_isAlive = false;
}
