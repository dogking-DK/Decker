#pragma once
#include "Base.h"

namespace dk {
inline void semiImplicitEuler(glm::vec3& x, glm::vec3& v, const glm::vec3& a, float dt)
{
    v += a * dt;
    x += v * dt;
}
}
