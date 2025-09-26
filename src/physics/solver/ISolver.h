// ISolver.h
#pragma once
#include "data/Particle.h"

class ISolver
{
public:
    virtual      ~ISolver() = default;
    virtual void solve(dk::ParticleData& particles, float dt) = 0;
};
