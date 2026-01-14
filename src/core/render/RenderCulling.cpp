#include "RenderCulling.h"

#include <glm/geometric.hpp>

namespace dk {
namespace {
    glm::vec4 normalize_plane(const glm::vec4& p)
    {
        glm::vec3 normal{p.x, p.y, p.z};
        float     length = glm::length(normal);
        if (length <= 0.0f)
        {
            return p;
        }
        return p / length;
    }
}

Frustum Frustum::fromMatrix(const glm::mat4& viewproj)
{
    Frustum frustum{};

    // Left, Right, Bottom, Top, Near, Far
    frustum.planes[0] = normalize_plane(glm::vec4(viewproj[0][3] + viewproj[0][0],
                                                 viewproj[1][3] + viewproj[1][0],
                                                 viewproj[2][3] + viewproj[2][0],
                                                 viewproj[3][3] + viewproj[3][0]));
    frustum.planes[1] = normalize_plane(glm::vec4(viewproj[0][3] - viewproj[0][0],
                                                 viewproj[1][3] - viewproj[1][0],
                                                 viewproj[2][3] - viewproj[2][0],
                                                 viewproj[3][3] - viewproj[3][0]));
    frustum.planes[2] = normalize_plane(glm::vec4(viewproj[0][3] + viewproj[0][1],
                                                 viewproj[1][3] + viewproj[1][1],
                                                 viewproj[2][3] + viewproj[2][1],
                                                 viewproj[3][3] + viewproj[3][1]));
    frustum.planes[3] = normalize_plane(glm::vec4(viewproj[0][3] - viewproj[0][1],
                                                 viewproj[1][3] - viewproj[1][1],
                                                 viewproj[2][3] - viewproj[2][1],
                                                 viewproj[3][3] - viewproj[3][1]));
    frustum.planes[4] = normalize_plane(glm::vec4(viewproj[0][2],
                                                 viewproj[1][2],
                                                 viewproj[2][2],
                                                 viewproj[3][2]));
    frustum.planes[5] = normalize_plane(glm::vec4(viewproj[0][3] - viewproj[0][2],
                                                 viewproj[1][3] - viewproj[1][2],
                                                 viewproj[2][3] - viewproj[2][2],
                                                 viewproj[3][3] - viewproj[3][2]));

    return frustum;
}

bool Frustum::intersectsSphere(const glm::vec3& center, float radius) const
{
    for (const auto& plane : planes)
    {
        const float distance = plane.x * center.x + plane.y * center.y + plane.z * center.z + plane.w;
        if (distance < -radius)
        {
            return false;
        }
    }
    return true;
}
} // namespace dk
