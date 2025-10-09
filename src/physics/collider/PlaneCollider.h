// PlaneCollider.h
#pragma once
#include "ICollider.h"

namespace dk {
class PlaneCollider : public ICollider
{
public:
    // 用平面法线和到原点的距离 d 来定义平面
    // 法线应该被归一化
    PlaneCollider(const vec3& normal, float distance)
        : normal(normalize(normal)), distance(distance)
    {
    }

    CollisionInfo testCollision(const vec3& particlePosition, float particleRadius) const override
    {
        CollisionInfo info;

        // 计算粒子中心到平面的有符号距离
        float signedDist = dot(particlePosition, normal) - distance;

        // 如果距离小于粒子半径，则发生碰撞
        if (signedDist < particleRadius)
        {
            info.hasCollided = true;
            info.normal      = normal;
            // 穿透深度是需要将粒子推回的距离
            info.penetrationDepth = particleRadius - signedDist;
        }

        return info;
    }

private:
    vec3  normal; // 平面的法线
    float distance; // 从原点沿法线到平面的距离 d
};
}
