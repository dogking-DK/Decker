// PBDSolver.h
#pragma once
#include "ISolver.h"
#include "data/Particle.h" // PBD求解器需要知道弹簧的连接关系

namespace dk {
class PBDSolver : public ISolver
{
public:
    PBDSolver(int solver_iterations = 5)
        : m_solverIterations(solver_iterations)
    {
    }

    void solve(dk::ParticleData& data, dk::Spring& springs, const float dt) override;

private:
    void predictPositions(ParticleData& data, dk::Spring& springs, float dt);


    void projectSpringConstraints(ParticleData& data, dk::Spring& springs);


    void updateVelocitiesAndPositions(ParticleData& data, dk::Spring& springs, float dt);


    int           m_solverIterations;
};
}
