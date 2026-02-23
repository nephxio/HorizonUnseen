#pragma once

#include "Entity.h"
#include "Systems/CollisionSystem.h"

class EnemyBullet : public Entity, public ICollidable {
public:
    EnemyBullet();
    
    void activate(float x, float y, float velocityX, float velocityY, float damage);
    void deactivate();
    
    void update(float deltaTime) override;
    bool isOffScreen() const;
    bool isActive() const { return m_active; }
    
    // ICollidable interface
    CollisionBox getCollisionBox() const override;
    void onCollision(ICollidable* other) override;
    float getDamage() const override { return Entity::getDamage(); }
    bool isAlive() const override { return m_active && Entity::isAlive(); }

private:
    bool m_active = false;
};
