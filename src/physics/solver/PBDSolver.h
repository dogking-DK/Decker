// PBDSolver.h
#pragma once
#include "ISolver.h"
#include "data/Particle.h" // PBD求解器需要知道弹簧的连接关系
#include "data/SpatialGrid.h"

namespace dk {
class PBDSolver : public ISolver
{
public:
    PBDSolver(int solver_iterations = 5, float cell_size = 0.1f)
        : m_solverIterations(solver_iterations), m_grid(cell_size)
    {
        
    }

    void solve(dk::ParticleData& data, dk::Spring& springs, const float dt) override;

private:
    void predictPositions(ParticleData& data, dk::Spring& springs, float dt);


    void projectSpringConstraints(ParticleData& data, dk::Spring& springs);

    void projectCollisionConstraints(ParticleData& data);

    void updateVelocitiesAndPositions(ParticleData& data, dk::Spring& springs, float dt);


    int           m_solverIterations;
    SpatialGrid m_grid;
};
}
