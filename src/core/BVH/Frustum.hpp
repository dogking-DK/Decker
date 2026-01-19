#pragma once

#include <array>
#include <glm/glm.hpp>
#include <vector>
#include <glm/gtc/matrix_access.hpp>

#include "AABB.hpp"

namespace dk {

struct Frustum
{
    std::array<glm::vec4, 6> planes{};

    bool contains(const AABB& box) const
    {
        if (!box.valid())
        {
            return true;
        }

        for (const auto& plane : planes)
        {
            const glm::vec3 normal(plane.x, plane.y, plane.z);
            const glm::vec3 p = {
                normal.x >= 0.0f ? box.max.x : box.min.x,
                normal.y >= 0.0f ? box.max.y : box.min.y,
                normal.z >= 0.0f ? box.max.z : box.min.z
            };
            if (glm::dot(normal, p) + plane.w < 0.0f)
            {
                return false;
            }
        }
        return true;
    }
};

inline Frustum make_frustum(const glm::mat4& view_proj)
{
    Frustum frustum;

    const glm::vec4 row0 = glm::row(view_proj, 0);
    const glm::vec4 row1 = glm::row(view_proj, 1);
    const glm::vec4 row2 = glm::row(view_proj, 2);
    const glm::vec4 row3 = glm::row(view_proj, 3);

    frustum.planes[0] = row3 + row0; // left
    frustum.planes[1] = row3 - row0; // right
    frustum.planes[2] = row3 + row1; // bottom
    frustum.planes[3] = row3 - row1; // top
    frustum.planes[4] = row3 + row2; // near
    frustum.planes[5] = row3 - row2; // far

    for (auto& p : frustum.planes)
    {
        const float len = glm::length(glm::vec3(p));
        if (len > 0.0f)
        {
            p /= len;
        }
    }

    return frustum;
}
}
