#include "PBDSolver.h"

namespace dk {
void PBDSolver::solve(ParticleData& data, Spring& springs, const float dt)
{
    // Step 1: Ԥ��λ��
    predictPositions(data, springs, dt);

    // Step 2: Լ�����ѭ��
    for (int i = 0; i < m_solverIterations; ++i)
    {
        projectSpringConstraints(data, springs);
        // δ��������������� projectCollisionConstraints(data);
    }

    // Step 3: �����ٶȺ�����λ��
    updateVelocitiesAndPositions(data, springs, dt);
}

void PBDSolver::predictPositions(ParticleData& data, Spring& springs, const float dt)
{
    const size_t count = data.size();

    for (size_t i = 0; i < count; ++i)
    {
        if (data.is_fixed[i]) continue;
        // �����ٶȲ�ʩ������
        vec3 velocity = (data.position[i] - data.previous_position[i]) / dt;
        velocity += data.force[i] * data.inv_mass[i] * dt;

        // Ԥ����λ�� (ע�⣺����ֻ���� position, prev_position ��ʱ����)
        data.position[i] += velocity * dt;
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

        // ����������
        float w1 = data.is_fixed[i1] ? 0.0f : 1.0f / data.mass[i1];
        float w2 = data.is_fixed[i2] ? 0.0f : 1.0f / data.mass[i2];
        if (w1 + w2 == 0.0f) continue;

        vec3  diff = p1 - p2;
        float dist = length(diff);
        if (dist == 0.0f) continue;

        // ����������
        float restLength = springs.rest_length[i];
        vec3  correction = (diff / dist) * (dist - restLength);
        //correction *= 0.1;
        float alpha = 1.0f - std::pow(1.0f - 0.9f, 1.0f / static_cast<float>(m_solverIterations));
        correction *= alpha;
        // Ӧ������
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

        // ��ͶӰ�������λ�� p_i ��֮ǰ֡��λ�� p_prev_i �����������ٶ�
        data.velocity[i] = (data.position[i] - data.previous_position[i]) / dt;

        // ���� "��һ֡" λ�ã�Ϊ��һ��ģ����׼��
        data.previous_position[i] = data.position[i];
    }
}
}
