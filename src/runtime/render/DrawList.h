#pragma once

#include <vector>

#include <vulkan/vulkan.hpp>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "resource/gpu/GPUMesh.h"
#include "resource/gpu/GPUMaterial.h"
#include "RenderTypes.h"

namespace dk::render {
struct DrawItem
{
    uint32_t                 proxy_index{0};
    std::shared_ptr<GPUMesh> mesh{};
    std::shared_ptr<GPUMaterial> material{};
    glm::mat4                world{1.0f};
    float                    depth{0.0f};
    // TODO: 升级到 GPU-driven 时，在这里加入 indirect 的参数索引与 instance 数据索引。
};

struct DrawLists
{
    std::vector<DrawItem> opaque;
    std::vector<DrawItem> outline;
    std::vector<DrawItem> transparent;

    void reset()
    {
        opaque.clear();
        outline.clear();
        transparent.clear();
    }
};

struct FrameContext
{
    glm::mat4 view{1.0f};
    glm::mat4 proj{1.0f};
    glm::mat4 viewproj{1.0f};
    glm::vec3 camera_position{0.0f};
    vk::Extent2D viewport{0, 0};
};

struct FrameStats
{
    size_t total_proxies{0};
    size_t visible_proxies{0};
    size_t opaque_draws{0};
};
} // namespace dk::render
