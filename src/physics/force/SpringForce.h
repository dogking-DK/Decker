// SpringForce.h
#pragma once
#include "IForce.h"

namespace dk {
// 弹簧力是一个特例，它需要粒子数据和弹簧拓扑数据
// 这里我们让它在构造时接收拓扑结构的引用
class SpringForce : public IForce
{
public:
    SpringForce(const Spring& topology) : _topology(topology)
    {
    }

    void applyForce(ParticleData& data) override
    {
        for (size_t i = 0; i < _topology.size(); ++i)
        {
            glm::vec3& posA = data.position[_topology.index_a[i]];
            glm::vec3& posB = data.position[_topology.index_b[i]];

            glm::vec3 direction = posB - posA;
            float     distance  = length(direction);
            if (distance == 0.0f) continue;

            float     forceMagnitude = -_topology.stiffness[i] * (distance - _topology.rest_length[i]);
            glm::vec3 force          = forceMagnitude * (direction / distance);

            if (!data.is_fixed[_topology.index_a[i]])
            {
                data.force[_topology.index_a[i]] -= force;
            }
            if (!data.is_fixed[_topology.index_b[i]])
            {
                data.force[_topology.index_b[i]] += force;
            }
        }
    }

private:
    const Spring& _topology;
};
}
