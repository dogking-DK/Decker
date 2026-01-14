#pragma once

#include <array>
#include <limits>

#include <glm/glm.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace dk::render {
struct AABB
{
    glm::vec3 min{std::numeric_limits<float>::max()};
    glm::vec3 max{-std::numeric_limits<float>::max()};

    bool valid() const
    {
        return min.x <= max.x && min.y <= max.y && min.z <= max.z;
    }

    glm::vec3 center() const
    {
        return 0.5f * (min + max);
    }

    glm::vec3 extents() const
    {
        return 0.5f * (max - min);
    }

    void expand(const glm::vec3& p)
    {
        min = glm::min(min, p);
        max = glm::max(max, p);
    }

    AABB transform(const glm::mat4& m) const
    {
        AABB result;
        if (!valid())
        {
            return result;
        }

        const glm::vec3 corners[] = {
            {min.x, min.y, min.z},
            {min.x, min.y, max.z},
            {min.x, max.y, min.z},
            {min.x, max.y, max.z},
            {max.x, min.y, min.z},
            {max.x, min.y, max.z},
            {max.x, max.y, min.z},
            {max.x, max.y, max.z},
        };

        for (const auto& c : corners)
        {
            const glm::vec4 world = m * glm::vec4(c, 1.0f);
            result.expand(glm::vec3(world));
        }
        return result;
    }
};

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

Frustum make_frustum(const glm::mat4& view_proj);
} // namespace dk::render
