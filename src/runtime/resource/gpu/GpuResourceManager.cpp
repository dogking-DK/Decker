#include "GpuResourceManager.h"

#include <algorithm>
#include <array>
#include <functional>
#include <numeric>

#include <glm/vec4.hpp>

#include "resource/cpu/MaterialLoader.h"
#include "resource/cpu/MeshLoader.h"
#include "resource/cpu/ResourceLoader.h"
#include "resource/cpu/TextureLoader.h"
#include "Vulkan/Buffer.h"
#include "Vulkan/CommandBuffer.h"
#include "Vulkan/CommandPool.h"
#include "Vulkan/Context.h"
#include "Vulkan/ImageView.h"
#include "Vulkan/Sampler.h"

using namespace dk::vkcore;

namespace dk {
namespace {

} // namespace

GpuResourceManager::GpuResourceManager(vkcore::VulkanContext& context, vkcore::CommandPool& command_pool,
                                       ResourceLoader& cpu_loader)
    : _context(context), _command_pool(command_pool), _cpu_loader(cpu_loader)
{
}

std::shared_ptr<GPUMesh> GpuResourceManager::uploadMeshData(const MeshData& mesh)
{
    const uint32_t vertex_count = mesh.vertex_count;
    const uint32_t index_count  = mesh.index_count;

    std::vector<GPUVertex> gpu_vertices(vertex_count);

    for (uint32_t i = 0; i < vertex_count; ++i)
    {
        GPUVertex v{};
        if (i < mesh.positions.size()) v.position = mesh.positions[i];
        if (i < mesh.normals.size()) v.normal = mesh.normals[i];
        if (i < mesh.tangents.size()) v.tangent = mesh.tangents[i];
        if (!mesh.texcoords.empty() && i < mesh.texcoords.front().size()) v.texcoord0 = mesh.texcoords.front()[i];
        if (!mesh.colors.empty() && i < mesh.colors.front().size()) v.color0 = mesh.colors.front()[i];
        gpu_vertices[i] = v;
    }

    auto staging = createBuffer(_context, sizeof(GPUVertex) * gpu_vertices.size(),
                                vk::BufferUsageFlagBits::eTransferSrc, VMA_MEMORY_USAGE_CPU_TO_GPU,
                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    staging->update(gpu_vertices.data(), sizeof(GPUVertex) * gpu_vertices.size());

    auto vertex_buffer = createBuffer(_context, sizeof(GPUVertex) * gpu_vertices.size(),
                                      vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
                                      VMA_MEMORY_USAGE_GPU_ONLY);

    auto index_staging = createBuffer(_context, sizeof(uint32_t) * mesh.indices.size(),
                                      vk::BufferUsageFlagBits::eTransferSrc, VMA_MEMORY_USAGE_CPU_TO_GPU,
                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    index_staging->update(mesh.indices.data(), sizeof(uint32_t) * mesh.indices.size());

    auto index_buffer = createBuffer(_context, sizeof(uint32_t) * mesh.indices.size(),
                                     vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
                                     VMA_MEMORY_USAGE_GPU_ONLY);

    submitAndWait(_context, _command_pool, [&](CommandBuffer& cmd) {
        vk::BufferCopy2 vertex_copy{0, 0, sizeof(GPUVertex) * gpu_vertices.size()};
        cmd.copyBuffer(*staging, *vertex_buffer, vertex_copy);

        vk::BufferCopy2 index_copy{0, 0, sizeof(uint32_t) * mesh.indices.size()};
        cmd.copyBuffer(*index_staging, *index_buffer, index_copy);
    });

    auto gpu_mesh           = std::make_shared<GPUMesh>();
    gpu_mesh->vertex_buffer = std::move(vertex_buffer);
    gpu_mesh->index_buffer  = std::move(index_buffer);
    gpu_mesh->vertex_count  = vertex_count;
    gpu_mesh->index_count   = index_count;
    return gpu_mesh;
}

std::shared_ptr<GPUTexture> GpuResourceManager::uploadTextureData(const TextureData& texture)
{
    const vk::DeviceSize image_size = texture.width * texture.height * texture.channels;
    auto staging                   = createBuffer(_context, image_size, vk::BufferUsageFlagBits::eTransferSrc,
                                                 VMA_MEMORY_USAGE_CPU_TO_GPU,
                                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    staging->update(texture.pixels.data(), image_size);

    ImageResource::Builder image_builder;
    image_builder.setFormat(vk::Format::eR8G8B8A8Unorm)
        .setExtent({texture.width, texture.height, 1})
        .setUsage(vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled);
    auto image = image_builder.buildUnique(_context);

    submitAndWait(_context, _command_pool, [&](CommandBuffer& cmd) {
        cmd.transitionImage(*image, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
                            vk::PipelineStageFlagBits2::eTopOfPipe, {},
                            vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite);

        vk::BufferImageCopy region{};
        region.imageExtent = vk::Extent3D(texture.width, texture.height, 1);
        region.imageSubresource.setAspectMask(vk::ImageAspectFlagBits::eColor);
        region.imageSubresource.setLayerCount(1);

        cmd.getHandle().copyBufferToImage(staging->getHandle(), image->getHandle(), vk::ImageLayout::eTransferDstOptimal,
                                          1, &region);

        cmd.transitionImage(*image, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
                            vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite,
                            vk::PipelineStageFlagBits2::eFragmentShader,
                            vk::AccessFlagBits2::eShaderRead);
    });

    auto view = ImageViewResource::create2D(_context, *image, vk::Format::eR8G8B8A8Unorm);
    auto samp = std::make_unique<SamplerResource>(_context, makeLinearClampSamplerInfo());

    auto gpu_tex   = std::make_shared<GPUTexture>();
    gpu_tex->image = std::move(image);
    gpu_tex->view  = std::move(view);
    gpu_tex->sampler = std::move(samp);
    gpu_tex->layout  = vk::ImageLayout::eShaderReadOnlyOptimal;
    return gpu_tex;
}

std::shared_ptr<GPUMesh> GpuResourceManager::loadMesh(UUID id)
{
    return _cache.resolve<GPUMesh>(id, [&] {
        auto cpu = _cpu_loader.load<MeshData>(id);
        if (!cpu) return std::shared_ptr<GPUMesh>{};
        return uploadMeshData(*cpu);
    });
}

std::shared_ptr<GPUTexture> GpuResourceManager::loadTexture(UUID id)
{
    return _cache.resolve<GPUTexture>(id, [&] {
        auto cpu = _cpu_loader.load<TextureData>(id);
        if (!cpu) return std::shared_ptr<GPUTexture>{};
        return uploadTextureData(*cpu);
    });
}

std::shared_ptr<GPUMaterial> GpuResourceManager::loadMaterial(UUID id)
{
    return _cache.resolve<GPUMaterial>(id, [&] {
        auto cpu = _cpu_loader.load<MaterialData>(id);
        if (!cpu) return std::shared_ptr<GPUMaterial>{};

        auto gpu = std::make_shared<GPUMaterial>();
        gpu->metallic   = cpu->metallic;
        gpu->roughness  = cpu->roughness;
        gpu->base_color = glm::vec4(
            cpu->base_color[0] / 255.0f,
            cpu->base_color[1] / 255.0f,
            cpu->base_color[2] / 255.0f,
            cpu->base_color[3] / 255.0f);

        if (cpu->base_color_tex) gpu->base_color_tex = uploadTextureData(*cpu->base_color_tex);
        if (cpu->metal_rough_tex) gpu->metal_rough_tex = uploadTextureData(*cpu->metal_rough_tex);
        if (cpu->normal_tex) gpu->normal_tex = uploadTextureData(*cpu->normal_tex);
        if (cpu->occlusion_tex) gpu->occlusion_tex = uploadTextureData(*cpu->occlusion_tex);
        if (cpu->emissive_tex) gpu->emissive_tex = uploadTextureData(*cpu->emissive_tex);
        return gpu;
    });
}
} // namespace dk
