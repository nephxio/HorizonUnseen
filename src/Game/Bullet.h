#pragma once

#include "Entity.h"
#include "Systems/CollisionSystem.h"

class Bullet : public Entity, public ICollidable {
public:
    Bullet(float x, float y, float velocityX, float velocityY, float damage);
    
    void update(float deltaTime) override;

    bool isOffScreen() const;
    
    // ICollidable interface
    CollisionBox getCollisionBox() const override;
    void onCollision(ICollidable* other) override;
    float getDamage() const override { return Entity::getDamage(); }
    bool isAlive() const override { return Entity::isAlive(); }
};
