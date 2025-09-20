// MassSpringSystem.cpp
#include "MassSpring.h"

// ���캯����ʼ��
MassSpringSystem::MassSpringSystem(glm::vec2 gravity, float damping)
    : _gravity(gravity), _damping_coefficient(damping)
{
}

int MassSpringSystem::addParticle(float mass, glm::vec2 position, bool is_pinned)
{
    Particle p;
    p.mass      = mass;
    p.position  = position;
    p.velocity  = glm::vec2(0.0f); // ��ʼ���ٶ�
    p.force     = glm::vec2(0.0f);    // ��ʼ����
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

    // ʹ�� glm::length ������������
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
    // 1. ���������
    for (auto& p : _particles)
    {
        p.force = glm::vec2(0.0f); // ʹ��GLM��������
    }

    // 2. ����ȫ���� (����������)
    for (auto& p : _particles)
    {
        if (!p.is_pinned)
        {
            p.force += _gravity * p.mass;
            p.force -= p.velocity * _damping_coefficient;
        }
    }

    // 3. ���㵯����
    for (const auto& [p1_idx, p2_idx, rest_length, stiffness] : _springs)
    {
        auto& p1 = _particles[p1_idx];
        auto& p2 = _particles[p2_idx];

        glm::vec2 vec_p1_to_p2 = p2.position - p1.position; // ��������������
        float     current_length = length(vec_p1_to_p2);  // ��ǰ����

        // �������غ�ʱ�������κβ������������
        if (current_length == 0.0f) continue;

        glm::vec2 direction = normalize(vec_p1_to_p2); // ��һ����

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
        // ʹ�� .y ���ʷ����ķ�ʽ��֮ǰһ��
        if (p.position.y < 0.0f)
        {
            p.position.y = 0.0f;
            p.velocity.y *= -0.5f;
        }
    }
}
