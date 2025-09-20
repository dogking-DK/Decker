// MassSpringSystem.cpp
#include "MassSpring.h"

// 构造函数初始化
MassSpringSystem::MassSpringSystem(glm::vec2 gravity, float damping)
    : _gravity(gravity), _damping_coefficient(damping)
{
}

int MassSpringSystem::addParticle(float mass, glm::vec2 position, bool is_pinned)
{
    Particle p;
    p.mass      = mass;
    p.position  = position;
    p.velocity  = glm::vec2(0.0f); // 初始化速度
    p.force     = glm::vec2(0.0f);    // 初始化力
    p.is_pinned = is_pinned;
    _particles.push_back(p);
    return _particles.size() - 1;
}

void MassSpringSystem::addSpring(const int p1_idx, const int p2_idx, const float stiffness)
{
    if (p1_idx >= _particles.size() || p2_idx >= _particles.size()) return;

    Spring s;
    s.p1_idx    = p1_idx;
    s.p2_idx    = p2_idx;
    s.stiffness = stiffness;

    const auto& p1 = _particles[p1_idx];
    const auto& p2 = _particles[p2_idx];

    // 使用 glm::length 计算向量长度
    s.rest_length = length(p1.position - p2.position);

    _springs.push_back(s);
}

void MassSpringSystem::update(const float dt)
{
    computeForces();
    integrate(dt);
    handleCollisions();
}

void MassSpringSystem::computeForces()
{
    // 1. 清空所有力
    for (auto& p : _particles)
    {
        p.force = glm::vec2(0.0f); // 使用GLM的零向量
    }

    // 2. 计算全局力 (重力和阻尼)
    for (auto& p : _particles)
    {
        if (!p.is_pinned)
        {
            p.force += _gravity * p.mass;
            p.force -= p.velocity * _damping_coefficient;
        }
    }

    // 3. 计算弹簧力
    for (const auto& [p1_idx, p2_idx, rest_length, stiffness] : _springs)
    {
        auto& p1 = _particles[p1_idx];
        auto& p2 = _particles[p2_idx];

        glm::vec2 vec_p1_to_p2 = p2.position - p1.position; // 计算两点间的向量
        float     current_length = length(vec_p1_to_p2);  // 当前长度

        // 当两点重合时，不做任何操作避免除以零
        if (current_length == 0.0f) continue;

        glm::vec2 direction = normalize(vec_p1_to_p2); // 归一方向

        float     force_magnitude = -stiffness * (current_length - rest_length);
        glm::vec2 spring_force    = direction * force_magnitude;

        if (!p1.is_pinned) p1.force -= spring_force;
        if (!p2.is_pinned) p2.force += spring_force;
    }
}

void MassSpringSystem::integrate(const float dt)
{
    for (auto& [position, velocity, force, mass, is_pinned] : _particles)
    {
        if (is_pinned) continue;

        glm::vec2 acceleration = force / mass;
        velocity += acceleration * dt;
        position += velocity * dt;
    }
}

void MassSpringSystem::handleCollisions()
{
    for (auto& p : _particles)
    {
        // 使用 .y 访问分量的方式和之前一样
        if (p.position.y < 0.0f)
        {
            p.position.y = 0.0f;
            p.velocity.y *= -0.5f;
        }
    }
}
