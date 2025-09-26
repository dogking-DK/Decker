// IForce.h
#pragma once
#include "data/Particle.h"

namespace dk {
class IForce
{
public:
    virtual ~IForce() = default;
    // 注意：现在 applyForce 直接操作数据容器
    virtual void applyForce(ParticleData& particles) = 0;
};
}
