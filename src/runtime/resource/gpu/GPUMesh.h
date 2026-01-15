#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace dk {
struct MeshData;

namespace vkcore {
    class VulkanContext;
    class BufferResource;
}

struct GPUVertex
{
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    glm::vec4 tangent{0.0f, 0.0f, 1.0f, 1.0f};
    glm::vec2 texcoord0{0.0f};
    glm::vec4 color0{1.0f};
};

struct GPUMesh
{
    std::unique_ptr<vkcore::BufferResource> vertex_buffer;
    std::unique_ptr<vkcore::BufferResource> index_buffer;
    uint32_t                                vertex_count{0};
    uint32_t                                index_count{0};
};

GPUMesh create_gpu_mesh_buffers(
    vkcore::VulkanContext& context,
    const MeshData& mesh);



} // namespace dk
