// GravityForce.h
#pragma once
#include "IForce.h"

namespace dk {

class GravityForce : public IForce
{
public:
    GravityForce(const glm::vec3& gravity) : gravity(gravity)
    {
    }

    void applyForce(ParticleData& data) override
    {
        const size_t count = data.size();
        for (size_t i = 0; i < count; ++i)
        {
            if (!data.is_fixed[i])
            {
                data.force[i] += data.mass[i] * gravity;
                //data.acceleration[i] += gravity;
            }
        }
    }

private:
    glm::vec3 gravity;
};
}
