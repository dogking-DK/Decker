#pragma once

#include <vk_types.h>
#include "Macros.h"
#include "Object.h"

namespace dk {
struct Bounds
{
    glm::vec3 origin; // 包围盒原点
    float     sphere_radius; // 包围盒同等包围球半径
    glm::vec3 extents; // 长宽高
    glm::vec3 max_edge; // 最大点
    glm::vec3 min_edge; // 最小点

    Bounds()
    {
        reset(); // 初始化时重置包围盒
    }


    bool isValid() const // 是否有效
    {
        return sphere_radius > 0 && extents.x > 0 && extents.y > 0 && extents.z > 0;
    }

    void extend(const Bounds& other) // 扩展包围盒
    {
        if (!other.isValid()) return; // 如果其他包围盒无效，则不进行扩展
        // 扩展包围盒到包含另一个包围盒
        min_edge     = min(min_edge, other.min_edge);
        max_edge     = max(max_edge, other.max_edge);
        origin       = (max_edge + min_edge) / 2.f;
        extents      = (max_edge - min_edge) / 2.f;
        sphere_radius = length(extents);
    }

    void reset() // 重置包围盒
    {
        origin                = {0, 0, 0};
        sphere_radius          = 0;
        extents               = {0, 0, 0};
        const auto& max_float = std::numeric_limits<float>::max();
        max_edge              = {-max_float, -max_float, -max_float};
        min_edge              = {max_float, max_float, max_float};
    }
};
} // dk
