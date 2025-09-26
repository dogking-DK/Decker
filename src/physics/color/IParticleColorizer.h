// IParticleColorizer.h
#pragma once
#include "data/Particle.h"

namespace dk {
class IParticleColorizer
{
public:
    virtual ~IParticleColorizer() = default;

    /**
     * @brief 根据内部逻辑更新 ParticleData 中的颜色.
     * @param data 要被着色的粒子数据.
     */
    virtual void colorize(ParticleData& data) = 0;
};
}
