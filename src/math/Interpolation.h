// Math/Interpolation.h
#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <algorithm> // for std::max/min

namespace dk { // 使用你自己的命名空间

/**
 * @brief 在两个值之间进行线性插值.
 *        这是对 glm::mix 的一个简单包装，使其在非GLM类型上也能工作.
 * @tparam T 值的类型 (可以是 float, vec3, vec4 等).
 * @param a 起始值.
 * @param b 结束值.
 * @param t 插值因子 [0, 1].
 * @return 插值结果.
 */
template <typename T>
T lerp(const T& a, const T& b, float t)
{
    return a * (1.0f - t) + b * t;
}

/**
 * @brief 对存储在一维向量中的三维场进行三线性插值采样.
 *        这个函数是模板化的，可以对任何支持 lerp 的类型进行插值.
 * @tparam T 场的数值类型 (e.g., float, glm::vec3).
 * @param field 存储场数据的一维向量.
 * @param dims 场的三维尺寸.
 * @param sample_pos 要进行采样的浮点坐标 (以网格单元为单位).
 * @return 插值得到的值.
 */
template <typename T>
T trilinearInterpolate(const std::vector<T>& field, const glm::ivec3& dims, const glm::vec3& sample_pos)
{
    // 1. 获取整数和小数部分
    int i = static_cast<int>(sample_pos.x);
    int j = static_cast<int>(sample_pos.y);
    int k = static_cast<int>(sample_pos.z);

    float tx = sample_pos.x - static_cast<float>(i);
    float ty = sample_pos.y - static_cast<float>(j);
    float tz = sample_pos.z - static_cast<float>(k);

    // 2. 钳制8个角点的坐标以防止越界
    int i0 = std::max(0, std::min(i, dims.x - 1));
    int i1 = std::max(0, std::min(i + 1, dims.x - 1));
    int j0 = std::max(0, std::min(j, dims.y - 1));
    int j1 = std::max(0, std::min(j + 1, dims.y - 1));
    int k0 = std::max(0, std::min(k, dims.z - 1));
    int k1 = std::max(0, std::min(k + 1, dims.z - 1));

    // 辅助函数：将 3D 坐标转换为 1D 向量的索引
    auto get_index = [&](int x, int y, int z)
    {
        return z * dims.x * dims.y + y * dims.x + x;
    };

    // 3. 从 field 中获取8个角点的值
    const T& c000 = field[get_index(i0, j0, k0)];
    const T& c100 = field[get_index(i1, j0, k0)];
    const T& c010 = field[get_index(i0, j1, k0)];
    const T& c110 = field[get_index(i1, j1, k0)];
    const T& c001 = field[get_index(i0, j0, k1)];
    const T& c101 = field[get_index(i1, j0, k1)];
    const T& c011 = field[get_index(i0, j1, k1)];
    const T& c111 = field[get_index(i1, j1, k1)];

    // 4. 执行七次线性插值
    T c00 = lerp(c000, c100, tx);
    T c10 = lerp(c010, c110, tx);
    T c01 = lerp(c001, c101, tx);
    T c11 = lerp(c011, c111, tx);

    T c0 = lerp(c00, c10, ty);
    T c1 = lerp(c01, c11, ty);

    return lerp(c0, c1, tz);
}
} // namespace dk::math
