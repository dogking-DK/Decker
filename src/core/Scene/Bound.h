#pragma once

#include <vk_types.h>
#include "Macros.h"
#include "Object.h"

namespace dk {

struct Bounds
{
    glm::vec3 origin;
    float sphereRadius;
    glm::vec3 extents;
    glm::vec3 max_edge;
    glm::vec3 min_edge;
};

} // dk
