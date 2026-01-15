// solver/StableFluidSolver.cpp
#include "solver/StableFliuidsSolver.h"
#include <cassert>
#include <cmath>
using namespace dk;

void StableFluidSolver::solve(ISimulationState& state, const float dt)
{
    auto* grid = dynamic_cast<MacGrid*>(&state);
    if (!grid) return;

    addForces(*grid, dt);
    diffuse(*grid, dt);
    advect(*grid, dt);
    project(*grid);
    applyBoundary(*grid);
}

void StableFluidSolver::addForces(MacGrid& g, float dt)
{
    // 重力只作用在 V 分量（y）上
    if (params_.gravity.y != 0.0f)
    {
        for (int k = 0; k < g.nz(); ++k)
            for (int j = 0; j < g.ny() + 1; ++j)
                for (int i = 0; i < g.nx(); ++i)
                {
                    g.V(i, j, k) += params_.gravity.y * dt;
                }
    }
    // 你可以在此添加外力场/鼠标吸力/湍流等
}

void StableFluidSolver::diffuse(MacGrid& g, float dt)
{
    if (params_.viscosity <= 0.0f) return;

    const float a = params_.viscosity * dt / (g.h() * g.h());
    // u: (nx+1, ny, nz)
    jacobiDiffuseComponent(g.u_tmp(), g.u(), g.nx() + 1, g.ny(), g.nz(), a, 1.0f / (1.0f + 6.0f * a),
                           params_.jacobi_iters);
    g.u().swap(g.u_tmp());
    // v: (nx, ny+1, nz)
    jacobiDiffuseComponent(g.v_tmp(), g.v(), g.nx(), g.ny() + 1, g.nz(), a, 1.0f / (1.0f + 6.0f * a),
                           params_.jacobi_iters);
    g.v().swap(g.v_tmp());
    // w: (nx, ny, nz+1)
    jacobiDiffuseComponent(g.w_tmp(), g.w(), g.nx(), g.ny(), g.nz() + 1, a, 1.0f / (1.0f + 6.0f * a),
                           params_.jacobi_iters);
    g.w().swap(g.w_tmp());
}

void StableFluidSolver::jacobiDiffuseComponent(std::vector<float>&       dst,
                                               const std::vector<float>& src,
                                               int                       sx, int  sy, int    sz,
                                               float                     a, float rbeta, int iters)
{
    std::vector<float> x = src; // 初值
    dst.resize(src.size());

    auto idx = [&](int i, int j, int k) { return (k * sy + j) * sx + i; };

    for (int it = 0; it < iters; ++it)
    {
        for (int k = 0; k < sz; ++k)
            for (int j = 0; j < sy; ++j)
                for (int i = 0; i < sx; ++i)
                {
                    // 边界：直接抄原值（免得访问越界）；可将其改为无滑移等
                    if (i == 0 || j == 0 || k == 0 || i == sx - 1 || j == sy - 1 || k == sz - 1)
                    {
                        dst[idx(i, j, k)] = 0.0f; // 粘墙
                        continue;
                    }
                    float sumN = x[idx(i - 1, j, k)] + x[idx(i + 1, j, k)]
                                 + x[idx(i, j - 1, k)] + x[idx(i, j + 1, k)]
                                 + x[idx(i, j, k - 1)] + x[idx(i, j, k + 1)];
                    // Jacobi: (x - a*laplace x = src) -> x = (src + a*sumN) * rbeta
                    dst[idx(i, j, k)] = (src[idx(i, j, k)] + a * sumN) * rbeta;
                }
        x.swap(dst);
    }
    // 结果在 x 中
    dst = x;
}

void StableFluidSolver::advect(MacGrid& g, float dt)
{
    semiLagrangianAdvectU(g, dt);
    semiLagrangianAdvectV(g, dt);
    semiLagrangianAdvectW(g, dt);
    if (params_.advect_dye)
    {
        semiLagrangianAdvectDye(g, dt);
    }
}

void StableFluidSolver::semiLagrangianAdvectU(MacGrid& g, float dt)
{
    // 回溯每个 u-face 的中心，沿全速度场跟踪
    for (int k = 0; k < g.nz(); ++k)
        for (int j = 0; j < g.ny(); ++j)
            for (int i = 0; i < g.nx() + 1; ++i)
            {
                vec3 x                     = g.uFacePos(i, j, k);
                vec3 v                     = g.sampleVelocity(x);
                vec3 xp                    = g.clampToDomain(x - v * dt);
                g.u_tmp()[g.idxU(i, j, k)] = g.sampleU(xp);
            }
    g.u().swap(g.u_tmp());
}

void StableFluidSolver::semiLagrangianAdvectV(MacGrid& g, float dt)
{
    for (int k = 0; k < g.nz(); ++k)
        for (int j = 0; j < g.ny() + 1; ++j)
            for (int i = 0; i < g.nx(); ++i)
            {
                vec3 x                     = g.vFacePos(i, j, k);
                vec3 v                     = g.sampleVelocity(x);
                vec3 xp                    = g.clampToDomain(x - v * dt);
                g.v_tmp()[g.idxV(i, j, k)] = g.sampleV(xp);
            }
    g.v().swap(g.v_tmp());
}

void StableFluidSolver::semiLagrangianAdvectW(MacGrid& g, float dt)
{
    for (int k = 0; k < g.nz() + 1; ++k)
        for (int j = 0; j < g.ny(); ++j)
            for (int i = 0; i < g.nx(); ++i)
            {
                vec3 x                     = g.wFacePos(i, j, k);
                vec3 v                     = g.sampleVelocity(x);
                vec3 xp                    = g.clampToDomain(x - v * dt);
                g.w_tmp()[g.idxW(i, j, k)] = g.sampleW(xp);
            }
    g.w().swap(g.w_tmp());
}

void StableFluidSolver::semiLagrangianAdvectDye(MacGrid& g, float dt)
{
    // 染料在 cell center 上
    for (int k = 0; k < g.nz(); ++k)
        for (int j = 0; j < g.ny(); ++j)
            for (int i = 0; i < g.nx(); ++i)
            {
                vec3 x                     = g.cellCenter(i, j, k);
                vec3 v                     = g.sampleVelocity(x);
                vec3 xp                    = g.clampToDomain(x - v * dt);
                g.p_tmp()[g.idxP(i, j, k)] = g.sampleCellScalar(g.dye(), xp);
            }
    g.dye().swap(g.p_tmp());
}

void StableFluidSolver::computeDivergence(MacGrid& g)
{
    const float invh = 1.0f / g.h();
    for (int k = 0; k < g.nz(); ++k)
        for (int j = 0; j < g.ny(); ++j)
            for (int i = 0; i < g.nx(); ++i)
            {
                float du       = g.U(i + 1, j, k) - g.U(i, j, k);
                float dv       = g.V(i, j + 1, k) - g.V(i, j, k);
                float dw       = g.W(i, j, k + 1) - g.W(i, j, k);
                g.Div(i, j, k) = invh * (du + dv + dw);
            }
}

void StableFluidSolver::jacobiPressure(MacGrid& g, int iters)
{
    // 解: laplace(p) = div, 6 点模板
    for (int it = 0; it < iters; ++it)
    {
        for (int k = 0; k < g.nz(); ++k)
            for (int j = 0; j < g.ny(); ++j)
                for (int i = 0; i < g.nx(); ++i)
                {
                    // 边界：设 p=0（可改 Neumann）
                    if (i == 0 || j == 0 || k == 0 || i == g.nx() - 1 || j == g.ny() - 1 || k == g.nz() - 1)
                    {
                        g.p_tmp()[g.idxP(i, j, k)] = 0.0f;
                        continue;
                    }

                    float sumN = g.P(i - 1, j, k) + g.P(i + 1, j, k)
                                 + g.P(i, j - 1, k) + g.P(i, j + 1, k)
                                 + g.P(i, j, k - 1) + g.P(i, j, k + 1);
                    // Jacobi: p_new = (sumN - h^2 * div) / 6
                    g.p_tmp()[g.idxP(i, j, k)] = (sumN - g.h() * g.h() * g.Div(i, j, k)) / 6.0f;
                }
        g.p().swap(g.p_tmp());
    }
}

void StableFluidSolver::subtractPressureGradient(MacGrid& g)
{
    const float invh = 1.0f / g.h();

    // u
    for (int k = 0; k < g.nz(); ++k)
        for (int j = 0; j < g.ny(); ++j)
            for (int i = 1; i < g.nx(); ++i)
            { // 注意 i=0 与 i=nx 的边界在 applyBoundary
                float gradp = g.P(i, j, k) - g.P(i - 1, j, k);
                g.U(i, j, k) -= invh * gradp;
            }
    // v
    for (int k = 0; k < g.nz(); ++k)
        for (int j = 1; j < g.ny(); ++j)
            for (int i = 0; i < g.nx(); ++i)
            {
                float gradp = g.P(i, j, k) - g.P(i, j - 1, k);
                g.V(i, j, k) -= invh * gradp;
            }
    // w
    for (int k = 1; k < g.nz(); ++k)
        for (int j = 0; j < g.ny(); ++j)
            for (int i = 0; i < g.nx(); ++i)
            {
                float gradp = g.P(i, j, k) - g.P(i, j, k - 1);
                g.W(i, j, k) -= invh * gradp;
            }
}

void StableFluidSolver::project(MacGrid& g)
{
    computeDivergence(g);
    jacobiPressure(g, params_.jacobi_iters);
    subtractPressureGradient(g);
}

void StableFluidSolver::applyBoundary(MacGrid& g)
{
    if (!params_.clamp_sides) return;

    // 粘墙：边界法向速度为0
    const int nx = g.nx(), ny = g.ny(), nz = g.nz();

    // u on x faces
    for (int j = 0; j < ny; ++j)
        for (int k = 0; k < nz; ++k)
        {
            g.U(0, j, k)  = 0.0f;
            g.U(nx, j, k) = 0.0f;
        }
    // v on y faces
    for (int i = 0; i < nx; ++i)
        for (int k = 0; k < nz; ++k)
        {
            g.V(i, 0, k)  = 0.0f;
            g.V(i, ny, k) = 0.0f;
        }
    // w on z faces
    for (int i = 0; i < nx; ++i)
        for (int j = 0; j < ny; ++j)
        {
            g.W(i, j, 0)  = 0.0f;
            g.W(i, j, nz) = 0.0f;
        }
}
