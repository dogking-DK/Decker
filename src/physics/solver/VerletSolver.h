#pragma once
#include "ISolver.h"

namespace dk {
class VerletSolver : public ISolver
{
public:
    void solve(dk::ISimulationState& state, const float dt) override;

    
};
}
