#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <cstdint>

// 轴对齐包围盒 (AABB)
struct AABB
{
    glm::vec3 min = glm::vec3(std::numeric_limits<float>::max());
    glm::vec3 max = glm::vec3(std::numeric_limits<float>::lowest());
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
AABB merge_aabbs(const AABB& a, const AABB& b)
{
    AABB merged;
    merged.min = min(a.min, b.min);
    merged.max = max(a.max, b.max);
    return merged;
}

// 预处理步骤：为每个三角形计算 AABB 和中心点
void preprocess_triangles(
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
