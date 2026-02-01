#include "Player.h"
#include "Systems/InputSystem.h"

Player::Player() : Entity(100.0f, 360.0f) {}

void Player::update(float deltaTime) {
    // Update position based on velocity
    m_position.x += m_velocity.x * deltaTime;
    m_position.y += m_velocity.y * deltaTime;

    // TODO: Clamp position to screen bounds
    // TODO: Handle shooting
    // TODO: Handle collisions
}

void Player::handleInput(const InputSystem& input, float deltaTime) {
    m_velocity = {0, 0};

    // WASD movement
    if (input.isKeyPressed(GLFW_KEY_W)) m_velocity.y -= m_speed;
    if (input.isKeyPressed(GLFW_KEY_S)) m_velocity.y += m_speed;
    if (input.isKeyPressed(GLFW_KEY_A)) m_velocity.x -= m_speed;
    if (input.isKeyPressed(GLFW_KEY_D)) m_velocity.x += m_speed;

    // Normalize diagonal movement
    if (m_velocity.x != 0 && m_velocity.y != 0) {
        float length = sqrt(m_velocity.x * m_velocity.x + m_velocity.y * m_velocity.y);
        m_velocity.x = (m_velocity.x / length) * m_speed;
        m_velocity.y = (m_velocity.y / length) * m_speed;
    }

    // TODO: Handle shooting with Space key
}
