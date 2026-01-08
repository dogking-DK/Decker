#include "GPUMesh.h"

#include "resource/cpu/MeshLoader.h"
#include "Vulkan/Buffer.h"

namespace dk {
GPUMesh create_gpu_mesh_buffers(vkcore::VulkanContext& context, const MeshData& mesh)
{
    GPUMesh gpu{};

    // 1. 组装 AoS 顶点数组
    std::vector<GPUVertex> vertices(mesh.vertex_count);
    for (uint32_t i = 0; i < mesh.vertex_count; ++i)
    {
        vertices[i].position = mesh.positions[i];
        vertices[i].normal = i < mesh.normals.size() ? mesh.normals[i] : glm::vec3(0, 1, 0);
        vertices[i].tangent = i < mesh.tangents.size() ? mesh.tangents[i] : glm::vec4(1, 0, 0, 1);
        if (!mesh.texcoords.empty() && i < mesh.texcoords[0].size())
        {
            //vertices[i].uv0 = mesh.texcoords[0][i];
        }
        else
        {
            //vertices[i].uv0 = glm::vec2(0.0f);
        }
    }

    gpu.vertex_buffer = vkcore::BufferResource::Builder()
    .setSize(vertices.size() * sizeof(GPUVertex))
    .setUsage(vk::BufferUsageFlagBits::eStorageBuffer)
    .buildUnique(context);
    gpu.vertex_buffer->update(vertices.data(), vertices.size());

    //// 2. 创建 vertex buffer（device local + staging 上传）
    //gpu.vertexBuffer = createDeviceLocalBufferWithData(
    //    context,
    //    vertices.data(),
    //    vertices.size() * sizeof(VertexGPU),
    //    vk::BufferUsageFlagBits::eVertexBuffer
    //    | vk::BufferUsageFlagBits::eTransferDst);

    //// 3. 创建 index buffer
    //gpu.indexBuffer = createDeviceLocalBufferWithData(
    //    context,
    //    mesh.indices.data(),
    //    mesh.indices.size() * sizeof(uint32_t),
    //    vk::BufferUsageFlagBits::eIndexBuffer
    //    | vk::BufferUsageFlagBits::eTransferDst);

    gpu.vertex_count = mesh.vertex_count;
    gpu.index_count = mesh.index_count;

    return gpu;
}

}
