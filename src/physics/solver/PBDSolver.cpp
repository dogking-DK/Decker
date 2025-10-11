#include "PBDSolver.h"

namespace dk {
void PBDSolver::solve(ParticleData& data, Spring& springs, const float dt)
{
    // Step 1: 预测位置
    predictPositions(data, springs, dt);

    // Step 2: 约束求解循环
    for (int i = 0; i < m_solverIterations; ++i)
    {
        projectSpringConstraints(data, springs);
        // 未来可以在这里调用 projectCollisionConstraints(data);
    }

    // Step 3: 更新速度和最终位置
    updateVelocitiesAndPositions(data, springs, dt);
}

void PBDSolver::predictPositions(ParticleData& data, Spring& springs, const float dt)
{
    const size_t count = data.size();

    for (size_t i = 0; i < count; ++i)
    {
        if (data.is_fixed[i]) continue;
        // 计算速度并施加外力，假设 force 已经被累加好了
        data.velocity[i] += data.force[i] * data.inv_mass[i] * dt;

        // 预测新位置 (注意：我们只更新 position, prev_position 暂时不变)
        data.position[i] += data.velocity[i] * dt;
    }
}


void PBDSolver::projectSpringConstraints(ParticleData& data, Spring& springs)
{
    for (size_t i = 0; i < springs.size(); ++i)
    {
        size_t i1 = springs.index_a[i];
        size_t i2 = springs.index_b[i];

        vec3& p1 = data.position[i1];
        vec3& p2 = data.position[i2];

        // 计算逆质量
        float w1 = data.is_fixed[i1] ? 0.0f : data.inv_mass[i1];
        float w2 = data.is_fixed[i2] ? 0.0f : data.inv_mass[i2];
        if (w1 + w2 == 0.0f) continue;

        vec3  diff = p1 - p2;
        float dist = length(diff);
        if (dist == 0.0f) continue;

        // 计算修正量
        float restLength = springs.rest_length[i];
        vec3  correction = (diff / dist) * (dist - restLength);
        //correction *= 0.1;
        float alpha = 1.0f - std::pow(1.0f - 0.9f, 1.0f / static_cast<float>(m_solverIterations));
        correction *= alpha;
        // 应用修正
        p1 -= (w1 / (w1 + w2)) * correction;
        p2 += (w2 / (w1 + w2)) * correction;
    }
}

void PBDSolver::updateVelocitiesAndPositions(ParticleData& data, Spring& springs, float dt)
{
    const size_t count = data.size();
    for (size_t i = 0; i < count; ++i)
    {
        if (data.is_fixed[i]) continue;

        // 用投影后的最终位置 p_i 和之前帧的位置 p_prev_i 来计算最终速度
        data.velocity[i] = (data.position[i] - data.previous_position[i]) / dt;

        float damping = 0.0005f;                 // 0.01~0.05
        float k = std::max(0.f, 1.f - damping);
        data.velocity[i] *= k;                 // ★ 指数阻尼，替代“阻尼力”
        if (glm::length(data.velocity[i]) < 0.0001f) data.velocity[i] = vec3(0.f);
        // 更新 "上一帧" 位置，为下一轮模拟做准备
        data.previous_position[i] = data.position[i];
    }
}
}
