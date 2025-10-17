#include "VerletSolver.h"

#include "data/Particle.h"

namespace dk {
void VerletSolver::solve(ISimulationState& state, const float dt)
{
    auto particle_state = dynamic_cast<ParticleSystemState*>(&state);
    auto data           = particle_state->particles;
    auto springs        = particle_state->springs;

    const float  dt_squared = dt * dt;
    const size_t count      = data.size();

    for (int i = 0; i < count; ++i)
    {
        if (data.is_fixed[i] || data.mass[i] == 0.0f) continue;

        // 1. 计算加速度
        // 注意: 在Verlet中，我们不改变全局的 data.accelerations
        vec3 acceleration = data.force[i] / data.mass[i];

        // 2. 记录当前位置，以便下一步使用
        vec3 temp_position = data.position[i];

        // 3. Verlet 积分核心公式
        // 新位置 = 2 * 当前位置 - 上一位置 + 加速度 * dt^2
        // 这里我们对公式做一点修改，将阻尼考虑进去，这被称为 "Velocity Verlet" 的一种形式
        data.position[i] = data.position[i]
                           + (data.position[i] - data.previous_position[i]) // 隐含的速度项 (p_current - p_prev)
                           + acceleration * dt_squared;

        // 4. 更新上一步位置
        data.previous_position[i] = temp_position;

        // 5. (可选但推荐) 更新速度，以便阻尼力等模块能获取到
        // v = (p_current - p_prev) / dt
        data.velocity[i] = (data.position[i] - data.previous_position[i]) / dt;
    }
}
}
