﻿#include "EulerSolver.h"

namespace dk {
void EulerSolver::solve(dk::ParticleData& data, dk::Spring& springs, const float dt)
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
}
