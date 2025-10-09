// Collision.h
#pragma once
#include "Base.h"

namespace dk {
// 存储一次碰撞检测的结果
struct CollisionInfo
{
    bool  hasCollided = false;
    vec3  normal;       // 碰撞点的法线，指向粒子应该被推离的方向
    float penetrationDepth; // 粒子穿入碰撞体的深度
};

// 任何碰撞体的通用接口
class ICollider
{
public:
    virtual ~ICollider() = default;

    // 检测一个粒子是否与该碰撞体发生碰撞
    // particleRadius 允许我们将粒子视为小球体，增加鲁棒性
    virtual CollisionInfo testCollision(const vec3& particlePosition, float particleRadius) const = 0;
};
}
