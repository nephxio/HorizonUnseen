#pragma once

#include "Entity.h"
#include "HealthSystem.h"
#include "Systems/CollisionSystem.h"

class InputSystem;

class Player : public Entity, public ICollidable {
public:
    Player();

    void update(float deltaTime) override;
    void handleInput(const InputSystem& input, float deltaTime);

    bool canShoot() const { return m_shootCooldown <= 0.0f; }
    void shoot();
    float getShootCooldown() const { return m_shootCooldown; }

    void queueDamage(float damage);
    void updateHealthSystem(float deltaTime);
    HealthSystem& getHealthSystem() { return m_healthSystem; }
    const HealthSystem& getHealthSystem() const { return m_healthSystem; }

    // ICollidable interface
    CollisionBox getCollisionBox() const override;
    void onCollision(ICollidable* other) override;
    float getDamage() const override { return 0.0f; }
    bool isAlive() const override { return Entity::isAlive(); }

private:
    float m_shootCooldown = 0.0f;
    HealthSystem m_healthSystem;
};
