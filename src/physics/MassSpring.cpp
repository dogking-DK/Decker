#include "MassSpring.h"
#include <execution>

void dk::SpringMassSystem::step(float dt)
{
    // 1. 清除旧力
    std::fill(std::execution::par_unseq, _data->force.begin(), _data->force.end(), glm::vec3(0.0f));

    // 2. 应用所有力模型
    for (auto& forceGen : _force)
    {
        forceGen->applyForce(*_data);
    }

    // 3. 使用自己的求解器进行积分
    _solver->solve(*_data, dt);
}

