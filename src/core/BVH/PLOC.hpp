#pragma once

#include <algorithm>
#include <execution>
#include <numeric>

#include <glm/glm.hpp>
#include "AABB.hpp"

// �򻯵� Morton ������� (��������ʾ)
uint32_t expandBits(uint32_t v)
{
    v = (v * 0x00010001u) & 0xFF0000FFu;
    v = (v * 0x00000101u) & 0x0F00F00Fu;
    v = (v * 0x00000011u) & 0xC30C30C3u;
    v = (v * 0x00000005u) & 0x49249249u;
    return v;
}

uint32_t morton3D(glm::vec3 pos)
{
    pos        = clamp(pos * 1023.0f, 0.0f, 1023.0f);
    uint32_t x = expandBits(static_cast<uint32_t>(pos.x));
    uint32_t y = expandBits(static_cast<uint32_t>(pos.y));
    uint32_t z = expandBits(static_cast<uint32_t>(pos.z));
    return x | (y << 1) | (z << 2);
}

// ���� AABB �ı����
float surface_area(const AABB& aabb)
{
    glm::vec3 d = aabb.max - aabb.min;
    return 2.0f * (d.x * d.y + d.y * d.z + d.z * d.x);
}


// ����������
std::vector<BVHNode> build_ploc_bvh(const std::vector<glm::vec3>& vertices, const std::vector<int>& indices)
{
    // === 1. ͼԪ��ȡ��Ԥ���� ===
    std::vector<AABB>      triangle_aabbs;
    std::vector<glm::vec3> triangle_centroids;
    preprocess_triangles(vertices, indices, triangle_aabbs, triangle_centroids);

    int num_triangles = triangle_aabbs.size();
    if (num_triangles == 0) return {};

    // �������������İ�Χ�У����� Morton ����Ĺ�һ��
    AABB scene_aabb;
    for (const auto& aabb : triangle_aabbs)
    {
        scene_aabb = merge_aabbs(scene_aabb, aabb);
    }
    glm::vec3 scene_extent = scene_aabb.max - scene_aabb.min;

    // === 2. Morton ���������� ===
    std::vector<uint32_t> morton_codes(num_triangles);
    for (int i = 0; i < num_triangles; ++i)
    {
        glm::vec3 normalized_pos = (triangle_centroids[i] - scene_aabb.min) / scene_extent;
        morton_codes[i]          = morton3D(normalized_pos);
    }

    std::vector<int> sorted_primitive_indices(num_triangles);
    std::iota(sorted_primitive_indices.begin(), sorted_primitive_indices.end(), 0); // Fills with 0, 1, 2, ...

    std::sort(std::execution::par, sorted_primitive_indices.begin(), sorted_primitive_indices.end(), [&](int a, int b)
    {
        return morton_codes[a] < morton_codes[b];
    });

    // === 3. ��ʼ������ (Ҷ�ӽڵ�) ===
    std::vector<BVHNode> bvh_nodes;
    bvh_nodes.resize(num_triangles * 2 - 1); // Ԥ�������п��ܵĽڵ�
    int node_count = num_triangles;

    std::vector<Cluster> clusters(num_triangles);
    for (int i = 0; i < num_triangles; ++i)
    {
        int original_idx = sorted_primitive_indices[i];

        bvh_nodes[i].aabb    = triangle_aabbs[original_idx];
        bvh_nodes[i].is_leaf = true;
        // ��Ҷ�ӽڵ��д洢ԭʼ����������
        bvh_nodes[i].left_child_index = original_idx;

        clusters[i] = {i, true};
    }

    int active_cluster_count = num_triangles;

    // === 4. �����ϲ� ===
    while (active_cluster_count > 1)
    {
        std::vector<int> nearest_neighbors(active_cluster_count, -1);

        // --- ����Ѱ������� ---
        // ע�⣺����һ���򻯵� O(N*K) ������K��������Χ����ʵʵ�ֻ���Ż���
        constexpr int search_range = 16;
        std::for_each(std::execution::par, clusters.begin(), clusters.end(), [&](Cluster& cluster)
        {
            if (!cluster.is_active) return;

            float min_sa            = std::numeric_limits<float>::max();
            int   best_neighbor_idx = -1;

            // Ϊ�˼򻯣�����ֱ���� clusters �����ϵ���
            // �ҵ���ǰ cluster �������е�λ��
            int current_idx = &cluster - &clusters[0];

            for (int i = 1; i < search_range; ++i)
            {
                // �������������
                int neighbor_indices[2] = {current_idx - i, current_idx + i};
                for (int neighbor_idx : neighbor_indices)
                {
                    if (neighbor_idx >= 0 && neighbor_idx < clusters.size() && clusters[neighbor_idx]. is_active)
                    {
                        AABB merged_aabb = merge_aabbs(bvh_nodes[cluster.node_index].aabb,
                                                       bvh_nodes[clusters[neighbor_idx].node_index].aabb);
                        float sa = surface_area(merged_aabb);
                        if (sa < min_sa)
                        {
                            min_sa            = sa;
                            best_neighbor_idx = neighbor_idx;
                        }
                    }
                }
            }
            if (best_neighbor_idx != -1)
            {
                nearest_neighbors[current_idx] = best_neighbor_idx;
            }
        });

        // --- �ϲ���Ϊ����ڵĶ� ---
        std::vector<int>  new_parent_node_indices;
        std::vector<bool> merged(active_cluster_count, false);

        for (int i = 0; i < active_cluster_count; ++i)
        {
            if (!clusters[i].is_active || merged[i]) continue;

            int neighbor_idx = nearest_neighbors[i];
            if (neighbor_idx != -1 && nearest_neighbors[neighbor_idx] == i)
            {
                // ȷ��ֻ�ϲ�һ�� (i, neighbor_idx) ��
                if (i > neighbor_idx) continue;

                int left_node_idx  = clusters[i].node_index;
                int right_node_idx = clusters[neighbor_idx].node_index;

                int parent_node_idx                          = node_count++;
                bvh_nodes[parent_node_idx].left_child_index  = left_node_idx;
                bvh_nodes[parent_node_idx].right_child_index = right_node_idx;
                bvh_nodes[parent_node_idx].aabb              = merge_aabbs(bvh_nodes[left_node_idx].aabb,
                                                              bvh_nodes[right_node_idx].aabb);
                bvh_nodes[parent_node_idx].is_leaf = false;

                bvh_nodes[left_node_idx].parent_index  = parent_node_idx;
                bvh_nodes[right_node_idx].parent_index = parent_node_idx;

                new_parent_node_indices.push_back(parent_node_idx);

                // ��Ǿɾ���Ϊ�ǻ
                clusters[i].is_active            = false;
                clusters[neighbor_idx].is_active = false;
                merged[i]                        = true;
                merged[neighbor_idx]             = true;
            }
        }

        // --- ѹ�������б� ---
        // (���Ǽ򻯵Ĵ���ѹ����GPU�ϻ��ò���ǰ׺��)
        std::vector<Cluster> next_clusters;
        for (const auto& cluster : clusters)
        {
            if (cluster.is_active)
            {
                next_clusters.push_back(cluster);
            }
        }
        for (int parent_idx : new_parent_node_indices)
        {
            next_clusters.push_back({parent_idx, true});
        }

        clusters             = next_clusters;
        active_cluster_count = clusters.size();
    }

    // ���ø��ڵ�
    if (!bvh_nodes.empty())
    {
        bvh_nodes.back().parent_index = -1; // ���һ�������Ľڵ��Ǹ�
    }

    bvh_nodes.resize(node_count); // ȥ�������Ԥ����ռ�
    return bvh_nodes;
}
