#include "Player.h"
#include "Systems/InputSystem.h"
#include "Config/GameConfig.h"
#include <cmath>

Player::Player() : Entity(100.0f, 360.0f, GameConfig::getInstance().playerHitPoints, 0.0f) {
}

void Player::update(float deltaTime) {
    // Update shoot cooldown
    if (m_shootCooldown > 0.0f) {
        m_shootCooldown -= deltaTime;
    }

    // Update position based on velocity
    m_position.x += m_velocity.x * deltaTime;
    m_position.y += m_velocity.y * deltaTime;

    // Clamp position to screen bounds
    auto& config = GameConfig::getInstance();
    float halfWidth = 20.0f;
    float halfHeight = 20.0f;

    if (m_position.x < halfWidth) m_position.x = halfWidth;
    if (m_position.x > config.screenWidth - halfWidth) m_position.x = config.screenWidth - halfWidth;
    if (m_position.y < halfHeight) m_position.y = halfHeight;
    if (m_position.y > config.screenHeight - halfHeight) m_position.y = config.screenHeight - halfHeight;
}

void Player::shoot() {
    auto& config = GameConfig::getInstance();
    m_shootCooldown = 1.0f / config.playerFireRate;
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
}

CollisionBox Player::getCollisionBox() const {
    // Position the box centered on the entity - BIGGER for easier testing
    float halfWidth = 30.0f;  // Increased from 20
    float halfHeight = 30.0f; // Increased from 20
    return CollisionBox{
        Vector2{m_position.x - halfWidth, m_position.y - halfHeight}, 
        halfWidth * 2.0f, 
        halfHeight * 2.0f
    };
}

void Player::onCollision(ICollidable* other) {
    if (other && other->getDamage() > 0) {
        takeDamage(other->getDamage());
    }
}
