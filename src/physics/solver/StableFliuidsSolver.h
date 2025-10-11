// StableFluidsSolver.h
#pragma once
#include "data/MACGrid.h"

namespace dk {
class StableFluidsSolver
{
public:
    // 主入口点，协调所有步骤
    void step(MACGrid& grid, float dt)
    {
        // 1. 将速度平流
        advectVelocity(grid, dt);

        // 2. 施加外力 (如重力)
        applyExternalForces(grid, dt);

        // 3. 压力投影，强制不可压缩性
        project(grid, dt);
    }

private:
    // --- 核心算法实现 ---

    // 使用半拉格朗日法更新速度场
    void advectVelocity(MACGrid& grid, float dt)
    {
        // 实现细节：
        // 1. 创建临时的 U, V, W 速度场副本
        // 2. 遍历每一个速度分量 (例如 m_u 中的每个值)
        // 3. 对于每个速度分量的位置，将其在当前速度场中 "往回" 追溯 dt 时间
        //    - back_traced_pos = current_pos - grid.sampleVelocity(current_pos) * dt;
        // 4. 使用 grid.sampleVelocity(back_traced_pos) 采样得到旧的速度
        // 5. 将这个旧速度写入到临时速度场副本中
        // 6. 循环结束后，用副本覆盖原始速度场
    }

    // 施加重力
    void applyExternalForces(MACGrid& grid, float dt)
    {
        constexpr float gravity = -9.8f;
        // 遍历所有 V 速度分量，加上 gravity * dt
        for (float& v_comp : grid.m_v)
        {
            v_comp += gravity * dt;
        }
    }

    // 压力求解和速度修正
    void project(MACGrid& grid, float dt)
    {
        // 这是最复杂的部分，分为三个子步骤：

        // A. 计算速度场的散度
        std::vector<float> divergence(grid.m_dims.x * grid.m_dims.y * grid.m_dims.z);
        // 遍历所有流体单元格，计算散度:
        // div = (u_right - u_left)/dx + (v_top - v_bottom)/dy + (w_front - w_back)/dz

        // B. 求解压力泊松方程
        // 我们需要迭代求解一个大型线性系统 Ap = div
        // 可以使用简单的雅可比迭代法 (Jacobi method):
        int pressure_iterations = 30;
        for (int i = 0; i < pressure_iterations; ++i)
        {
            // 遍历所有流体单元格 (i, j, k)，根据其邻居的压力和散度来更新当前压力
            // p_new[i,j,k] = (divergence[i,j,k] + p_old[i+1,j,k] + p_old[i-1,j,k] + ... ) / 6.0f;
        }

        // C. 用压力梯度修正速度
        // 遍历所有速度分量，减去压力梯度
        // 例如 u_new[i,j,k] = u_old[i,j,k] - dt * (pressure[i,j,k] - pressure[i-1,j,k]) / grid.m_cellSize;
    }
};
}
