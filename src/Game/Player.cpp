#include "Player.h"
#include "Systems/InputSystem.h"
#include "Config/GameConfig.h"
#include <cmath>

Player::Player() : Entity(100.0f, 360.0f) {
    m_health = GameConfig::getInstance().playerHitPoints;
}

void Player::update(float deltaTime) {
    // Update position based on velocity
    m_position.x += m_velocity.x * deltaTime;
    m_position.y += m_velocity.y * deltaTime;

    // TODO: Clamp position to screen bounds
    // TODO: Handle shooting
    // TODO: Handle collisions
}

void Player::handleInput(const InputSystem& input, float deltaTime) {
    auto& config = GameConfig::getInstance();

    m_velocity = {0, 0};

    // WASD and Arrow key movement
    if (input.isKeyPressed(GLFW_KEY_W) || input.isKeyPressed(GLFW_KEY_UP)) 
        m_velocity.y -= config.playerMovementSpeedY;
    if (input.isKeyPressed(GLFW_KEY_S) || input.isKeyPressed(GLFW_KEY_DOWN)) 
        m_velocity.y += config.playerMovementSpeedY;
    if (input.isKeyPressed(GLFW_KEY_A) || input.isKeyPressed(GLFW_KEY_LEFT)) 
        m_velocity.x -= config.playerMovementSpeedX;
    if (input.isKeyPressed(GLFW_KEY_D) || input.isKeyPressed(GLFW_KEY_RIGHT)) 
        m_velocity.x += config.playerMovementSpeedX;

    // Normalize diagonal movement
    if (m_velocity.x != 0 && m_velocity.y != 0) {
        float length = sqrt(m_velocity.x * m_velocity.x + m_velocity.y * m_velocity.y);
        float speedX = config.playerMovementSpeedX;
        float speedY = config.playerMovementSpeedY;
        float averageSpeed = (speedX + speedY) / 2.0f;
        m_velocity.x = (m_velocity.x / length) * averageSpeed;
        m_velocity.y = (m_velocity.y / length) * averageSpeed;
    }

    // TODO: Handle shooting with Space key
}
