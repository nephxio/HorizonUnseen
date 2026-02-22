#pragma once

#include "Entity.h"
#include "Systems/CollisionSystem.h"

enum class EnemyState {
    MovingLeft,
    Diving
};

class Enemy : public Entity, public ICollidable {
public:
    Enemy(float x, float y);

    void update(float deltaTime) override;

    bool isOffScreen() const;
    EnemyState getState() const { return m_state; }

    // ICollidable interface
    CollisionBox getCollisionBox() const override;
    void onCollision(ICollidable* other) override;
    float getDamage() const override { return Entity::getDamage(); }
    bool isAlive() const override { return Entity::isAlive(); }

private:
    EnemyState m_state;
    float m_diveThreshold;
};
