#pragma once

#include <glm/vec3.hpp>

namespace dk::render {
struct FluidRenderData
{
    // TODO: 接入 GPU 纹理/缓冲区句柄（密度/速度场、粒子数据）
    glm::vec3 bounds_min{0.0f};
    glm::vec3 bounds_max{0.0f};
};

struct VoxelRenderData
{
    // TODO: 接入 SDF/密度纹理或表面网格句柄
    glm::vec3 bounds_min{0.0f};
    glm::vec3 bounds_max{0.0f};
};
} // namespace dk::render
