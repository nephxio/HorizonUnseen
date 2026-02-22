#pragma once

#include "Game/Entity.h"
#include <cmath>

struct CollisionBox {
    Vector2 position;
    float width;
    float height;
    
    bool intersects(const CollisionBox& other) const {
        return (position.x < other.position.x + other.width &&
                position.x + width > other.position.x &&
                position.y < other.position.y + other.height &&
                position.y + height > other.position.y);
    }
};

class ICollidable {
public:
    virtual ~ICollidable() = default;
    
    virtual CollisionBox getCollisionBox() const = 0;
    virtual void onCollision(ICollidable* other) = 0;
    virtual bool isAlive() const = 0;
    virtual float getDamage() const = 0;
};

class CollisionSystem {
public:
    static bool checkCollision(const CollisionBox& a, const CollisionBox& b) {
        return a.intersects(b);
    }
    
    static float distance(Vector2 a, Vector2 b) {
        float dx = b.x - a.x;
        float dy = b.y - a.y;
        return std::sqrt(dx * dx + dy * dy);
    }
    
    static bool circleCollision(Vector2 posA, float radiusA, Vector2 posB, float radiusB) {
        return distance(posA, posB) < (radiusA + radiusB);
    }
};
