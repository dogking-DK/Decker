// system/FluidSystem.h
#pragma once
#include "World.h"            // ISystem 定义在这里
#include "solver/StableFliuidsSolver.h"
#include "data/MacGrid.h"

namespace dk {
class FluidSystem : public ISystem
{
public:
    struct Config
    {
        int                       nx     = 64, ny = 32, nz = 32;
        float                     h      = 0.02f;
        vec3                      origin = vec3(0);
        StableFluidSolver::Params solver_params{};
    };

    explicit FluidSystem(const Config& cfg = Config{})
        : grid_(cfg.nx, cfg.ny, cfg.nz, cfg.h, cfg.origin),
          solver_(cfg.solver_params)
    {
        // 可以在此给 dye 初值/速度激励，做个简单烟雾源
        int cx = cfg.nx / 4, cy = cfg.ny / 2, cz = cfg.nz / 2;
        for (int k = cz - 2; k <= cz + 2; ++k)
            for (int j = cy - 2; j <= cy + 2; ++j)
                for (int i = cx - 2; i <= cx + 2; ++i)
                {
                    if (i >= 0 && i < cfg.nx && j >= 0 && j < cfg.ny && k >= 0 && k < cfg.nz)
                    {
                        grid_.Dye(i, j, k) = 1.0f; // 一小团染料
                    }
                }
        // 向右侧加点初速度（演示）
        for (int j = 0; j < cfg.ny; ++j)
            for (int k = 0; k < cfg.nz; ++k) grid_.U(1, j, k) = 1.0f;
    }

    void step(float dt) override
    {
        solver_.solve(grid_, dt);
    }

    void getRenderData(std::vector<PointData>& out_data) const override
    {
        // 这里先留空（避免假设你的 PointData 结构）
        // 你可以把高于阈值的染料体素转换为点云（position=color=中心，alpha=浓度）
        out_data.clear();
    }

    MacGrid&       grid() { return grid_; }
    const MacGrid& grid() const { return grid_; }

    StableFluidSolver&       solver() { return solver_; }
    const StableFluidSolver& solver() const { return solver_; }

private:
    MacGrid           grid_;
    StableFluidSolver solver_;
};
}
