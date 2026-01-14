#pragma once

#include <cstdint>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "UUID.hpp"
#include "vk_types.h"

namespace dk {
struct DrawItem
{
    MaterialPipeline* pipeline{nullptr};
    MaterialInstance* material{nullptr};
    const GPUMeshBuffers* mesh{nullptr};

    VkBuffer        index_buffer{VK_NULL_HANDLE};
    uint32_t        index_count{0};
    uint32_t        first_index{0};
    VkDeviceAddress vertex_buffer_address{0};

    glm::mat4 world_transform{1.0f};
    float     depth{0.0f};
    UUID      node_id{};

    // TODO: 升级到 indirect 时在此处填充实例/对象索引与 GPU ObjectBuffer 偏移。
};

struct DrawListBucket
{
    std::vector<DrawItem> items;

    void clear() { items.clear(); }
    bool empty() const { return items.empty(); }
};

struct DrawLists
{
    DrawListBucket shadow;
    DrawListBucket opaque;
    DrawListBucket transparent;

    void clear()
    {
        shadow.clear();
        opaque.clear();
        transparent.clear();
    }
};

struct FrameContext
{
    glm::mat4 view{1.0f};
    glm::mat4 proj{1.0f};
    glm::mat4 viewproj{1.0f};
    glm::vec3 camera_position{0.0f};
    VkExtent2D draw_extent{};
};
} // namespace dk
