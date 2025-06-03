#pragma once

#include <vk_types.h>
#include "Macros.h"
#include "Object.h"

namespace dk {
struct Bounds
{
    glm::vec3 origin; // ��Χ��ԭ��
    float     sphere_radius; // ��Χ��ͬ�Ȱ�Χ��뾶
    glm::vec3 extents; // �����
    glm::vec3 max_edge; // ����
    glm::vec3 min_edge; // ��С��

    Bounds()
    {
        reset(); // ��ʼ��ʱ���ð�Χ��
    }


    bool isValid() const // �Ƿ���Ч
    {
        return sphere_radius > 0 && extents.x > 0 && extents.y > 0 && extents.z > 0;
    }

    void extend(const Bounds& other) // ��չ��Χ��
    {
        if (!other.isValid()) return; // ���������Χ����Ч���򲻽�����չ
        // ��չ��Χ�е�������һ����Χ��
        min_edge     = min(min_edge, other.min_edge);
        max_edge     = max(max_edge, other.max_edge);
        origin       = (max_edge + min_edge) / 2.f;
        extents      = (max_edge - min_edge) / 2.f;
        sphere_radius = length(extents);
    }

    void reset() // ���ð�Χ��
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
