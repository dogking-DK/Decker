#pragma once
#include "Base.h"
#include <vector>

namespace dk {
struct PointData
{
    glm::vec4 position;
    glm::vec4 color;
};


// 使用SoA (Struct of Arrays) 存储质点数据
// 优点: 内存访问连续，缓存命中率高，非常适合SIMD优化
struct ParticleData
{
    std::vector<glm::vec3> position; // 位置
    std::vector<glm::vec3> previous_position; // 上一步位置，用于计算速度
    std::vector<glm::vec3> velocity; // 速度
    std::vector<glm::vec3> acceleration; // 加速度
    std::vector<glm::vec3> force; // 受力
    std::vector<float>     mass; // 质量
    std::vector<float>     inv_mass; // 质量的倒数，方便计算加速度
    std::vector<vec4>      color; // 质量的倒数，方便计算加速度
    std::vector<bool>      is_fixed; // 是否固定

    // 添加一个新质点，并返回其索引
    size_t addParticle(const glm::vec3& pos, const float m, const bool fixed = false)
    {
        position.emplace_back(pos);
        previous_position.emplace_back(pos);
        velocity.emplace_back(0.0f);
        acceleration.emplace_back(0.0f);
        force.emplace_back(0.0f);
        mass.push_back(m);
        inv_mass.push_back(1.0f / m);
        color.emplace_back(0, 0, 0, 1);
        is_fixed.push_back(fixed);
        return position.size() - 1;
    }

    size_t size() const
    {
        return position.size();
    }

    bool empty() const
    {
        return position.empty();
    }
};

// 弹簧的拓扑结构数据
struct Spring
{
    std::vector<size_t> index_a; // 连接的质点索引A
    std::vector<size_t> index_b; // 连接的质点索引B
    std::vector<float>  stiffness; // 弹性系数
    std::vector<float>  rest_length; // 自然长度

    size_t size() const
    {
        return index_a.size();
    }

    void addSpring(size_t idxA, size_t idxB, float k, float l)
    {
        index_a.push_back(idxA);
        index_b.push_back(idxB);
        stiffness.push_back(k);
        rest_length.push_back(l);
    }
};
}
