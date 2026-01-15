// ISolver.h
#pragma once
#include "Base.h"

class ISolver
{
public: 
    virtual      ~ISolver() = default;
    virtual void solve(dk::ISimulationState& state, const float dt) = 0;
};
