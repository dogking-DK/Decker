#include "VerletSolver.h"

namespace dk {
void VerletSolver::solve(dk::ParticleData& data, dk::Spring& springs, float dt)
{
    const float  dt_squared = dt * dt;
    const size_t count      = data.size();

    for (int i = 0; i < count; ++i)
    {
        if (data.is_fixed[i] || data.mass[i] == 0.0f) continue;

        // 1. ������ٶ�
        // ע��: ��Verlet�У����ǲ��ı�ȫ�ֵ� data.accelerations
        vec3 acceleration = data.force[i] / data.mass[i];

        // 2. ��¼��ǰλ�ã��Ա���һ��ʹ��
        vec3 temp_position = data.position[i];

        // 3. Verlet ���ֺ��Ĺ�ʽ
        // ��λ�� = 2 * ��ǰλ�� - ��һλ�� + ���ٶ� * dt^2
        // �������ǶԹ�ʽ��һ���޸ģ������ῼ�ǽ�ȥ���ⱻ��Ϊ "Velocity Verlet" ��һ����ʽ
        data.position[i] = data.position[i]
                           + (data.position[i] - data.previous_position[i]) // �������ٶ��� (p_current - p_prev)
                           + acceleration * dt_squared;

        // 4. ������һ��λ��
        data.previous_position[i] = temp_position;

        // 5. (��ѡ���Ƽ�) �����ٶȣ��Ա���������ģ���ܻ�ȡ��
        // v = (p_current - p_prev) / dt
        data.velocity[i] = (data.position[i] - data.previous_position[i]) / dt;
    }
}
}
