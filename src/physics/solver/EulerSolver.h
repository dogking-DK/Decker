// EulerSolver.h
#pragma once
#include "ISolver.h"

namespace dk {

class EulerSolver : public ISolver
{
public:
    void solve(ParticleData& data, float dt) override
    {
        const size_t count = data.size();
        for (size_t i = 0; i < count; ++i)
        {
            if (data.is_fixed[i] || data.mass[i] == 0.0f) continue;

            data.acceleration[i] = data.force[i] / data.mass[i];
            data.velocity[i] += data.acceleration[i] * dt;
            data.position[i] += data.velocity[i] * dt;
        }
    }
};
}
