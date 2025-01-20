#pragma once

#include <vk_types.h>
#include "Macros.h"
#include "Object.h"

DECKER_START

struct Bounds
{
    glm::vec3 origin;
    float sphereRadius;
    glm::vec3 extents;
    glm::vec3 max_edge;
    glm::vec3 min_edge;
};

DECKER_END
