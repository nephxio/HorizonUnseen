#pragma once

#include "Entity.h"
#include "Systems/CollisionSystem.h"

class InputSystem;

class Player : public Entity, public ICollidable {
public:
    Player();

    void update(float deltaTime) override;
    void handleInput(const InputSystem& input, float deltaTime);

    // ICollidable interface
    CollisionBox getCollisionBox() const override;
    void onCollision(ICollidable* other) override;
    float getDamage() const override { return Entity::getDamage(); }
    bool isAlive() const override { return Entity::isAlive(); }
};
