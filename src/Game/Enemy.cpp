#include "Enemy.h"
#include "Config/GameConfig.h"
#include <cmath>

Enemy::Enemy(float x, float y) 
    : Entity(x, y)
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
