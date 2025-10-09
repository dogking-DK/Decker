// EulerSolver.h
#pragma once
#include "ISolver.h"

namespace dk {

class EulerSolver : public ISolver
{
public:
    void solve(dk::ParticleData& data, dk::Spring& springs, float dt) override;

};
}
