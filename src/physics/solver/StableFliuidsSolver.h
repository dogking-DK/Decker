// solver/StableFluidSolver.h
#pragma once
#include "ISolver.h"
#include "data/MacGrid.h"

namespace dk {

class StableFluidSolver : public ISolver
{
public:
    struct Params
    {
        float    viscosity     = 0.0005f;      // 黏性系数 (m^2/s)
        dk::vec3 gravity       = dk::vec3(0, -9.8f, 0);
        int      jacobi_iters  = 60;           // 压力/扩散迭代次数
        bool     clamp_sides   = true;         // 盒边界“粘墙”
        bool     advect_dye    = true;         // 是否对流染料
        float    vorticity_eps = 0.0f;         // 涡度加强(0关闭)
    };

    explicit StableFluidSolver(const Params& p = Params{}) : params_(p)
    {
    }

    void solve(dk::ISimulationState& state, float dt) override;

    void          setParams(const Params& p) { params_ = p; }
    const Params& params() const { return params_; }

private:
    // pipeline
    void addForces(dk::MacGrid& g, float dt);
    void diffuse(dk::MacGrid& g, float dt);
    void advect(dk::MacGrid& g, float dt);
    void project(dk::MacGrid& g);
    void applyBoundary(dk::MacGrid& g);

    // kernels
    void jacobiDiffuseComponent(std::vector<float>&       dst,
                                const std::vector<float>& src,
                                int                       sx, int      sy, int sz,
                                float                     alpha, float rbeta,
                                int                       iters);

    void semiLagrangianAdvectU(dk::MacGrid& g, float dt);
    void semiLagrangianAdvectV(dk::MacGrid& g, float dt);
    void semiLagrangianAdvectW(dk::MacGrid& g, float dt);
    void semiLagrangianAdvectDye(dk::MacGrid& g, float dt);

    void computeDivergence(dk::MacGrid& g);
    void jacobiPressure(dk::MacGrid& g, int iters);
    void subtractPressureGradient(dk::MacGrid& g);

    Params params_;
};

}