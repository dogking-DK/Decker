#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <cstdint>

namespace dk {

// 轴对齐包围盒 (AABB)
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

    void expand(const AABB& aabb)
    {
        expand(aabb.min);
        expand(aabb.max);
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

// BVH 节点
// 使用一个大的 vector 来存储所有节点，通过索引来引用
struct BVHNode
{
    AABB aabb;
    int  parent_index      = -1;
    int  left_child_index  = -1; // 对于叶子节点，这里可以存储图元索引
    int  right_child_index = -1;
    bool is_leaf           = false;
};

// 构建过程中使用的临时聚类信息
struct Cluster
{
    int  node_index;  // 指向 BVHNode 存储中的索引
    bool is_active = true;
};


// 辅助函数：合并两个 AABB
inline AABB merge_aabbs(const AABB& a, const AABB& b)
{
    AABB merged;
    merged.min = min(a.min, b.min);
    merged.max = max(a.max, b.max);
    return merged;
}

// 预处理步骤：为每个三角形计算 AABB 和中心点
inline void preprocess_triangles(
    const std::vector<glm::vec3>& vertices,
    const std::vector<int>&       indices,
    std::vector<AABB>&            triangle_aabbs,
    std::vector<glm::vec3>&       triangle_centroids)
{
    int num_triangles = indices.size() / 3;
    triangle_aabbs.resize(num_triangles);
    triangle_centroids.resize(num_triangles);

    for (int i = 0; i < num_triangles; ++i)
    {
        glm::vec3 v0 = vertices[indices[i * 3 + 0]];
        glm::vec3 v1 = vertices[indices[i * 3 + 1]];
        glm::vec3 v2 = vertices[indices[i * 3 + 2]];

        AABB aabb;
        aabb.min = min(min(v0, v1), v2);
        aabb.max = max(max(v0, v1), v2);

        triangle_aabbs[i]     = aabb;
        triangle_centroids[i] = (aabb.min + aabb.max) * 0.5f;
    }
}
}
