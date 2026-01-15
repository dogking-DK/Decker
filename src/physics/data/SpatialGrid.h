// SpatialGrid.h
#pragma once
#include "Particle.h"
#include <unordered_map>
#include <vector>
#include <tsl/robin_map.h>

namespace dk {
// 用于哈希 glm::ivec3 的辅助结构体
struct Ivec3Hash
{
    std::size_t operator()(const glm::ivec3& v) const
    {
        // 一个简单的哈希函数
        return std::hash<int>()(v.x) ^ (std::hash<int>()(v.y) << 1) ^ (std::hash<int>()(v.z) << 2);
    }
};

class SpatialGrid
{
public:
    SpatialGrid(float cell_size) : m_cellSize(cell_size)
    {
    }

    // 根据所有粒子的当前位置，重新构建哈希表
    void build(const ParticleData& data)
    {
        m_grid.clear();
        for (size_t i = 0; i < data.size(); ++i)
        {
            glm::ivec3 cell_idx = getCellIndex(data.position[i]);
            m_grid[cell_idx].push_back(i);
        }
    }

    // 查询一个粒子周围可能发生碰撞的其他粒子的索引
    void query(const ParticleData& data, size_t particle_idx, std::vector<size_t>& out_candidates)
    {
        out_candidates.clear();
        glm::ivec3 center_idx = getCellIndex(data.position[particle_idx]);

        // 遍历中心单元格和周围26个邻居
        for (int x = -1; x <= 1; ++x)
        {
            for (int y = -1; y <= 1; ++y)
            {
                for (int z = -1; z <= 1; ++z)
                {
                    glm::ivec3 neighbor_idx = center_idx + glm::ivec3(x, y, z);
                    auto       it           = m_grid.find(neighbor_idx);
                    if (it != m_grid.end())
                    {
                        // 将邻居单元格中的所有粒子添加到候选列表中
                        out_candidates.insert(out_candidates.end(), it->second.begin(), it->second.end());
                    }
                }
            }
        }
    }

private:
    glm::ivec3 getCellIndex(const glm::vec3& pos) const
    {
        return glm::ivec3(
            static_cast<int>(std::floor(pos.x / m_cellSize)),
            static_cast<int>(std::floor(pos.y / m_cellSize)),
            static_cast<int>(std::floor(pos.z / m_cellSize))
        );
    }

    float                                                      m_cellSize;
    tsl::robin_map<glm::ivec3, std::vector<size_t>, Ivec3Hash> m_grid;
};
}
