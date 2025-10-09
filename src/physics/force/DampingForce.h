// DampingForce.h
#pragma once
#include "IForce.h"

namespace dk {
class DampingForce : public IForce
{
public:
    // 阻尼系数 _constant_factor, 一个小的正数 (例如 0.1 到 1.0)
    DampingForce(float damping_coefficient) : _constant_factor(damping_coefficient)
    {
    }

    void applyForce(ParticleData& data) override
    {
        const size_t count = data.size();
        for (size_t i = 0; i < count; ++i)
        {
            if (!data.is_fixed[i])
            {
                data.force[i] -= _constant_factor * data.velocity[i];
            }
        }
    }

private:
    float _constant_factor;
};
}
