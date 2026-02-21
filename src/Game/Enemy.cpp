#include "Enemy.h"
#include <cmath>

Enemy::Enemy(float x, float y) 
    : Entity(x, y)
    , m_state(EnemyState::MovingLeft)
{
    // Calculate 1/3 of the screen width (assuming 1280 width)
    m_diveThreshold = 1280.0f * (2.0f / 3.0f);
    
    // Initial velocity moving left
    m_velocity.x = -m_horizontalSpeed;
    m_velocity.y = 0.0f;
}

void Enemy::update(float deltaTime) {
    // State transitions
    if (m_state == EnemyState::MovingLeft && m_position.x <= m_diveThreshold) {
        m_state = EnemyState::Diving;
        
        // Calculate dive velocity at 30 degree angle
        // Convert angle to radians
        float angleRad = m_diveAngle * (3.14159265f / 180.0f);
        
        // Velocity components for diving down-left at 30 degrees
        m_velocity.x = -m_diveSpeed * cos(angleRad);
        m_velocity.y = m_diveSpeed * sin(angleRad);
    }
    
    // Update position based on velocity
    m_position.x += m_velocity.x * deltaTime;
    m_position.y += m_velocity.y * deltaTime;
}

bool Enemy::isOffScreen() const {
    // Check if enemy is off screen (below or far left)
    return m_position.y > 720.0f || m_position.x < -100.0f;
}
