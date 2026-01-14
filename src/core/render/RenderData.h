#pragma once

#include <glm/vec3.hpp>
#include <vulkan/vulkan.h>

#include "UUID.hpp"
#include "Scene/Bound.h"

namespace dk {
struct FluidRenderData
{
    VkImageView density_texture{VK_NULL_HANDLE};
    VkImageView velocity_texture{VK_NULL_HANDLE};
    VkBuffer    params_buffer{VK_NULL_HANDLE};
    Bounds      bounds{};
};

struct VoxelRenderData
{
    VkImageView sdf_texture{VK_NULL_HANDLE};
    VkBuffer    params_buffer{VK_NULL_HANDLE};
    Bounds      bounds{};
    UUID        surface_mesh{};
};
} // namespace dk
