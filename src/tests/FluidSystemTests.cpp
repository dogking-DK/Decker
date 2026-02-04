#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>

#include "physics/data/MacGrid.h"
#include "physics/fluid/FluidSystem.h"
#include "physics/solver/StableFliuidsSolver.h"

namespace {
struct TestContext
{
    int total  = 0;
    int failed = 0;

    void expect(bool ok, const std::string& name)
    {
        ++total;
        if (!ok)
        {
            ++failed;
            std::cout << "[FAIL] " << name << "\n";
        }
        else
        {
            std::cout << "[PASS] " << name << "\n";
        }
    }
};

bool nearlyEqual(float a, float b, float eps = 1e-5f)
{
    return std::fabs(a - b) <= eps;
}

void testFluidSystemInitialState(TestContext& t)
{
    dk::FluidSystem::Config cfg;
    cfg.nx = 8;
    cfg.ny = 6;
    cfg.nz = 6;
    cfg.h  = 1.0f;
    cfg.solver_params.gravity     = dk::vec3(0.0f);
    cfg.solver_params.viscosity   = 0.0f;
    cfg.solver_params.advect_dye  = false;
    cfg.solver_params.clamp_sides = false;

    dk::FluidSystem system(cfg);
    const dk::MacGrid& grid = system.grid();

    t.expect(grid.nx() == cfg.nx && grid.ny() == cfg.ny && grid.nz() == cfg.nz,
             "FluidSystem config applied to grid dimensions");

    float max_dye = 0.0f;
    for (float v : grid.dye()) max_dye = std::max(max_dye, v);
    t.expect(max_dye > 0.5f, "FluidSystem seeds initial dye blob");

    bool all_u_one = true;
    for (int k = 0; k < grid.nz(); ++k)
        for (int j = 0; j < grid.ny(); ++j)
        {
            if (!nearlyEqual(grid.U(1, j, k), 1.0f))
            {
                all_u_one = false;
                break;
            }
        }
    t.expect(all_u_one, "FluidSystem seeds initial U velocity at i=1");
}

void testStableFluidSolverGravity(TestContext& t)
{
    dk::MacGrid grid(4, 4, 4, 1.0f);

    dk::StableFluidSolver::Params params;
    params.gravity     = dk::vec3(0.0f, -9.8f, 0.0f);
    params.viscosity   = 0.0f;
    params.advect_dye  = false;
    params.clamp_sides = false;
    params.jacobi_iters = 8;

    dk::StableFluidSolver solver(params);
    const float           dt = 0.1f;
    solver.solve(grid, dt);

    const float expected = params.gravity.y * dt;
    bool        all_match = true;
    for (int k = 0; k < grid.nz(); ++k)
        for (int j = 0; j < grid.ny() + 1; ++j)
            for (int i = 0; i < grid.nx(); ++i)
            {
                if (!nearlyEqual(grid.V(i, j, k), expected))
                {
                    all_match = false;
                    break;
                }
            }

    t.expect(all_match, "StableFluidSolver applies gravity to V component");
}

void testStableFluidSolverClampSides(TestContext& t)
{
    dk::MacGrid grid(3, 3, 3, 1.0f);

    for (int j = 0; j < grid.ny(); ++j)
        for (int k = 0; k < grid.nz(); ++k)
        {
            grid.U(0, j, k)        = 2.0f;
            grid.U(grid.nx(), j, k) = 2.0f;
        }

    for (int i = 0; i < grid.nx(); ++i)
        for (int k = 0; k < grid.nz(); ++k)
        {
            grid.V(i, 0, k)         = 2.0f;
            grid.V(i, grid.ny(), k) = 2.0f;
        }

    for (int i = 0; i < grid.nx(); ++i)
        for (int j = 0; j < grid.ny(); ++j)
        {
            grid.W(i, j, 0)         = 2.0f;
            grid.W(i, j, grid.nz()) = 2.0f;
        }

    dk::StableFluidSolver::Params params;
    params.gravity     = dk::vec3(0.0f);
    params.viscosity   = 0.0f;
    params.advect_dye  = false;
    params.clamp_sides = true;
    params.jacobi_iters = 4;

    dk::StableFluidSolver solver(params);
    solver.solve(grid, 0.0f);

    bool boundaries_zero = true;

    for (int j = 0; j < grid.ny(); ++j)
        for (int k = 0; k < grid.nz(); ++k)
        {
            boundaries_zero = boundaries_zero
                              && nearlyEqual(grid.U(0, j, k), 0.0f)
                              && nearlyEqual(grid.U(grid.nx(), j, k), 0.0f);
        }

    for (int i = 0; i < grid.nx(); ++i)
        for (int k = 0; k < grid.nz(); ++k)
        {
            boundaries_zero = boundaries_zero
                              && nearlyEqual(grid.V(i, 0, k), 0.0f)
                              && nearlyEqual(grid.V(i, grid.ny(), k), 0.0f);
        }

    for (int i = 0; i < grid.nx(); ++i)
        for (int j = 0; j < grid.ny(); ++j)
        {
            boundaries_zero = boundaries_zero
                              && nearlyEqual(grid.W(i, j, 0), 0.0f)
                              && nearlyEqual(grid.W(i, j, grid.nz()), 0.0f);
        }

    t.expect(boundaries_zero, "StableFluidSolver clamps boundary velocities");
}
} // namespace

int main()
{
    TestContext t;
    testFluidSystemInitialState(t);
    testStableFluidSolverGravity(t);
    testStableFluidSolverClampSides(t);

    std::cout << "\nTotal: " << t.total << ", Failed: " << t.failed << "\n";
    return t.failed == 0 ? 0 : 1;
}
