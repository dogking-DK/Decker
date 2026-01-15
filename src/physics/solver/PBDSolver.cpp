#include "PBDSolver.h"

namespace dk {
void PBDSolver::solve(dk::ISimulationState& state, const float dt)
{
    auto particle_state = dynamic_cast<ParticleSystemState*>(&state);
    auto data = particle_state->particles;
    auto springs = particle_state->springs;
    // Step 1: 预测位置
    predictPositions(data, springs, dt);

    m_grid.build(data); // 更新空间哈希网格

    // Step 2: 约束求解循环
    for (int i = 0; i < m_solverIterations; ++i)
    {
        projectSpringConstraints(data, springs);
        // 未来可以在这里调用 projectCollisionConstraints(data);
        projectCollisionConstraints(data);
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

void PBDSolver::projectCollisionConstraints(ParticleData& data)
{
    constexpr float     thickness    = 0.1f; // 布料厚度
    const float         thickness_sq = thickness * thickness;
    std::vector<size_t> candidates;

    for (size_t i = 0; i < data.size(); ++i)
    {
        // 1. 使用空间哈希获取候选粒子
        m_grid.query(data, i, candidates);

        for (size_t j_idx : candidates)
        {
            // 2. 避免重复计算和自我检测
            if (i >= j_idx) continue;

            // 3. 精确检测和投影
            vec3& p1 = data.position[i];
            vec3& p2 = data.position[j_idx];

            vec3  diff    = p1 - p2;
            float dist_sq = dot(diff, diff);

            if (dist_sq < thickness_sq)
            {
                // 发生碰撞，进行投影
                float dist       = std::sqrt(dist_sq);
                vec3  correction = (diff / dist) * (thickness - dist);

                float w1 = data.is_fixed[i] ? 0.0f : data.inv_mass[i];
                float w2 = data.is_fixed[j_idx] ? 0.0f : data.inv_mass[j_idx];
                if (w1 + w2 == 0.0f) continue;

                // 与弹簧约束完全相同的投影逻辑！
                p1 += (w1 / (w1 + w2)) * correction;
                p2 -= (w2 / (w1 + w2)) * correction;
            }
        }
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
        float k       = std::max(0.f, 1.f - damping);
        data.velocity[i] *= k;                 // ★ 指数阻尼，替代“阻尼力”
        if (length(data.velocity[i]) < 0.0001f) data.velocity[i] = vec3(0.f);
        // 更新 "上一帧" 位置，为下一轮模拟做准备
        data.previous_position[i] = data.position[i];
    }
}
}
