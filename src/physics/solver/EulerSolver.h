// EulerSolver.h
#pragma once
#include "ISolver.h"

namespace dk {

class EulerSolver : public ISolver
{
public:
    void solve(dk::ISimulationState& state, const float dt) override;

};
}
