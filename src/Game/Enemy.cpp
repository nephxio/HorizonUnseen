#include "Enemy.h"
#include "Config/GameConfig.h"
#include <cmath>

Enemy::Enemy(float x, float y) 
    : Entity(x, y, GameConfig::getInstance().enemyHitPoints, GameConfig::getInstance().enemyDamage)
    , m_state(EnemyState::MovingLeft)
{
    auto& config = GameConfig::getInstance();

    // Calculate 1/3 of the screen width
    m_diveThreshold = config.screenWidth * (2.0f / 3.0f);

    // Initial velocity moving left
    m_velocity.x = -config.enemyHorizontalSpeed;
    m_velocity.y = 0.0f;
}

void Enemy::update(float deltaTime) {
    // Update shoot cooldown
    if (m_shootCooldown > 0.0f) {
        m_shootCooldown -= deltaTime;
    }

    auto& config = GameConfig::getInstance();

    // State transitions
    if (m_state == EnemyState::MovingLeft && m_position.x <= m_diveThreshold) {
        m_state = EnemyState::Diving;

        // Calculate dive velocity at configured angle
        float angleRad = config.enemyDiveAngle * (3.14159265f / 180.0f);

        // Velocity components for diving down-left
        m_velocity.x = -config.enemyDiveSpeed * cos(angleRad);
        m_velocity.y = config.enemyDiveSpeed * sin(angleRad);
    }

    // Update position based on velocity
    m_position.x += m_velocity.x * deltaTime;
    m_position.y += m_velocity.y * deltaTime;
}

bool Enemy::isOffScreen() const {
    auto& config = GameConfig::getInstance();
    // Check if enemy is off screen (below or far left)
    return m_position.y > config.screenHeight || m_position.x < -100.0f;
}

CollisionBox Enemy::getCollisionBox() const {
    // Position the box centered on the entity - BIGGER for easier testing
    float halfWidth = 25.0f;  // Increased from 15
    float halfHeight = 25.0f; // Increased from 15
    return CollisionBox{
        Vector2{m_position.x - halfWidth, m_position.y - halfHeight}, 
        halfWidth * 2.0f, 
        halfHeight * 2.0f
    };
}

void Enemy::onCollision(ICollidable* other) {
    if (other && other->getDamage() > 0) {
        takeDamage(other->getDamage());
    }
}
