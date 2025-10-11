// MACGrid.h
#pragma once
#include "Base.h" // 你的基础头文件，包含 glm 等
#include <vector>

namespace dk {
// 定义网格单元格的类型
enum class CellType : uint8_t
{
    AIR,    // 空气
    FLUID,  // 流体
    SOLID   // 固体/边界
};

class MACGrid
{
public:
    MACGrid(const glm::ivec3& dimensions, float cell_size)
        : m_dims(dimensions), m_cellSize(cell_size)
    {
        // 速度分量 U 存储在 (i+1/2, j, k) 处，所以 X 维度多一个
        m_u.resize((m_dims.x + 1) * m_dims.y * m_dims.z, 0.0f);
        // 速度分量 V 存储在 (i, j+1/2, k) 处，所以 Y 维度多一个
        m_v.resize(m_dims.x * (m_dims.y + 1) * m_dims.z, 0.0f);
        // 速度分量 W 存储在 (i, j, k+1/2) 处，所以 Z 维度多一个
        m_w.resize(m_dims.x * m_dims.y * (m_dims.z + 1), 0.0f);

        // 压力和类型存储在单元格中心
        m_pressure.resize(m_dims.x * m_dims.y * m_dims.z, 0.0f);
        m_cellType.resize(m_dims.x * m_dims.y * m_dims.z, CellType::AIR);
    }

    // --- 公共接口 ---

    // 在世界空间中的任意位置对速度场进行三线性插值采样
    // 这是平流步骤的关键
    glm::vec3 sampleVelocity(const glm::vec3& world_pos) const
    {
        // 实现细节：
        // 1. 将 world_pos 转换为网格坐标 (浮点数)
        // 2. 分别对 U, V, W 速度分量进行三线性插值
        //    - 采样 U 需要在其交错网格的8个邻居点上插值
        //    - 采样 V, W 同理
        // 3. 返回插值得到的 (u, v, w) 向量
        return glm::vec3(0.0f); // 待实现
    }

    const glm::ivec3& getDimensions() const { return m_dims; }
    float             getCellSize() const { return m_cellSize; }

    // --- 数据成员 (为方便访问设为 public) ---

    glm::ivec3 m_dims;
    float      m_cellSize;

    // 速度场 (交错存储)
    std::vector<float> m_u, m_v, m_w;

    // 单元格中心数据
    std::vector<float>    m_pressure;
    std::vector<CellType> m_cellType;
};
}
