// SystemGenerators.h
#pragma once

#include "MassSpring.h"

namespace dk {
// 使用属性结构体 (Properties Struct) 来传递参数，这比长长的函数参数列表更清晰
struct ClothProperties
{
    int       width_segments  = 10;
    int       height_segments = 10;
    float     width           = 10.0f;
    float     height          = 10.0f;
    glm::vec3 start_position  = vec3(0.0f);

    // 不同类型的弹簧有不同的刚度，这能产生更真实的布料效果
    float stiffness_structural = 500.0f; // 结构弹簧 (上下左右)
    float stiffness_shear      = 100.0f; // 剪切弹簧 (对角线)
    float stiffness_bend       = 50.0f;  // 弯曲弹簧 (跨一个粒子)

    float mass_per_particle = 0.1f;
    bool  pin_top_corners   = true;
};

struct RopeProperties
{
    vec3  start_position    = vec3(0.0f);
    vec3  end_position      = vec3(10.0f, 0.0f, 0.0f);
    int   num_segments      = 10;
    float stiffness         = 800.0f;
    float mass_per_particle = 0.1f;
    bool  pin_start         = true;
    bool  pin_end           = false;
};

/**
 * @brief 在一个 SpringMassSystem 中生成一块布料网格.
 * @param system 要被填充的系统.
 * @param props 布料的属性.
 */
inline void create_cloth(SpringMassSystem& system, const ClothProperties& props)
{
    ParticleData& particles = system.getParticles_mut();
    Spring&       topology  = system.getTopology_mut();

    // 记录添加前的粒子数量，以便正确计算索引
    const size_t base_index  = particles.size();
    const int    grid_width  = props.width_segments + 1;
    const int    grid_height = props.height_segments + 1;

    // 1. 添加粒子
    for (int y = 0; y < grid_height; ++y)
    {
        for (int x = 0; x < grid_width; ++x)
        {
            vec3 pos = props.start_position + vec3(
                           static_cast<float>(x) / static_cast<float>(props.width_segments) * props.width,
                           0.0f,
                           static_cast<float>(y) / static_cast<float>(props.height_segments) * -props.height // Z轴负方向
                       );
            particles.addParticle(pos, props.mass_per_particle);
        }
    }

    // 用于根据网格坐标获取粒子索引的辅助函数
    auto get_index = [&](const int x, const int y)
    {
        return base_index + grid_width * y + x;
    };

    // 2. 添加弹簧
    for (int y = 0; y < grid_height; ++y)
    {
        for (int x = 0; x < grid_width; ++x)
        {
            // 结构弹簧 (Structural)
            if (x < props.width_segments)
            { // 水平
                topology.addSpring(get_index(x, y), get_index(x + 1, y), props.stiffness_structural,
                                   props.width / static_cast<float>(props.width_segments));
            }
            if (y < props.height_segments)
            { // 垂直
                topology.addSpring(get_index(x, y), get_index(x, y + 1), props.stiffness_structural,
                                   props.height / static_cast<float>(props.height_segments));
            }

            //// 剪切弹簧 (Shear)
            //if (x < props.width_segments && y < props.height_segments)
            //{
            //    const float diag_length = length(vec2(props.width / static_cast<float>(props.width_segments),
            //                                          props.height / static_cast<float>(props.height_segments)));
            //    topology.addSpring(get_index(x, y), get_index(x + 1, y + 1), props.stiffness_shear, diag_length);
            //    topology.addSpring(get_index(x + 1, y), get_index(x, y + 1), props.stiffness_shear, diag_length);
            //}

            //// 弯曲弹簧 (Bend) - 这是让布料抗弯曲的关键
            //if (x < props.width_segments - 1)
            //{
            //    topology.addSpring(get_index(x, y), get_index(x + 2, y), props.stiffness_bend,
            //                       2.0f * props.width / static_cast<float>(props.width_segments));
            //}
            //if (y < props.height_segments - 1)
            //{
            //    topology.addSpring(get_index(x, y), get_index(x, y + 2), props.stiffness_bend,
            //                       2.0f * props.height / static_cast<float>(props.height_segments));
            //}
        }
    }

    // 3. 固定点 (Pinning)
    if (props.pin_top_corners)
    {
        for (int i = 0; i <=  props.width_segments; ++i)
        {
            particles.is_fixed[get_index(i, 0)] = true;

        }

        particles.is_fixed[get_index(0, 0)]                    = true;
        particles.is_fixed[get_index(props.width_segments, 0)] = true;
    }
}


/**
 * @brief 在一个 SpringMassSystem 中生成一条绳索 (或软棒).
 * @param system 要被填充的系统.
 * @param props 绳索的属性.
 */
inline void create_rope(SpringMassSystem& system, const RopeProperties& props)
{
    ParticleData& particles = system.getParticles_mut();
    Spring&       topology  = system.getTopology_mut();

    const size_t base_index    = particles.size();
    const int    num_particles = props.num_segments + 1;

    const vec3  direction      = props.end_position - props.start_position;
    const float total_length   = length(direction);
    const float segment_length = total_length / static_cast<float>(props.num_segments);

    // 1. 添加粒子
    for (int i = 0; i < num_particles; ++i)
    {
        vec3 pos = props.start_position + (direction / total_length) * (segment_length * i);

        particles.addParticle(pos, props.mass_per_particle);
    }

    // 2. 添加弹簧
    for (int i = 0; i < props.num_segments; ++i)
    {
        topology.addSpring(base_index + i, base_index + i + 1, props.stiffness, segment_length);
    }

    // 3. 固定点
    if (props.pin_start)
    {
        particles.is_fixed[base_index] = true;
    }
    if (props.pin_end)
    {
        particles.is_fixed[base_index + props.num_segments] = true;
    }
}
}
