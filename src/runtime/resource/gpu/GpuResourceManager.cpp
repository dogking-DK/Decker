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
#include "Vulkan/DescriptorSetLayout.h"
#include "Vulkan/DescriptorSetPool.h"
#include "Vulkan/DescriptorWriter.h"
#include "Vulkan/ImageView.h"
#include "Vulkan/Sampler.h"

using namespace dk::vkcore;

namespace dk {
namespace {
} // namespace

GpuResourceManager::GpuResourceManager(VulkanContext&  ctx, UploadContext& upload_ctx,
                                       ResourceLoader& cpu_loader)
    : _context(ctx), _upload_ctx(upload_ctx), _cpu_loader(cpu_loader)
{
}

GpuResourceManager::~GpuResourceManager()
{
}

void GpuResourceManager::setMaterialDescriptorLayout(vkcore::DescriptorSetLayout* layout)
{
    _material_descriptor_layout = layout;
    if (!_material_descriptor_allocator)
    {
        std::vector<vk::DescriptorPoolSize> pool_sizes = {
            {vk::DescriptorType::eCombinedImageSampler, 512}
        };
        _material_descriptor_allocator = std::make_unique<vkcore::GrowableDescriptorAllocator>(
            &_context, 256, pool_sizes);
    }
    if (_default_material)
    {
        ensureMaterialDescriptorSet(*_default_material);
    }
}

std::shared_ptr<GPUMaterial> GpuResourceManager::getDefaultMaterial()
{
    if (_default_material)
    {
        return _default_material;
    }

    auto material            = std::make_shared<GPUMaterial>();
    material->base_color     = glm::vec4(1.0f);
    material->base_color_tex = getDefaultWhiteTexture();
    ensureMaterialDescriptorSet(*material);
    _default_material = material;
    return _default_material;
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

    auto gpu_mesh = std::make_shared<GPUMesh>();
    gpu_mesh->vertex_buffer = BufferResource::Builder()
                             .setSize(sizeof(GPUVertex) * gpu_vertices.size())
                             .setUsage(vk::BufferUsageFlagBits::eVertexBuffer |
                                       vk::BufferUsageFlagBits::eTransferDst)
                             .withVmaRequiredFlags(vk::MemoryPropertyFlagBits::eDeviceLocal)
                             .withVmaUsage(VMA_MEMORY_USAGE_AUTO)
                             .buildUnique(_context);
    upload_buffer_data_immediate(_upload_ctx, gpu_vertices.data(), sizeof(GPUVertex) * gpu_vertices.size(), *gpu_mesh->
                                 vertex_buffer, 0, BufferUsage::VertexBuffer);

    gpu_mesh->index_buffer = BufferResource::Builder()
                            .setSize(sizeof(uint32_t) * mesh.indices.size())
                            .setUsage(vk::BufferUsageFlagBits::eIndexBuffer |
                                      vk::BufferUsageFlagBits::eTransferDst)
                            .withVmaRequiredFlags(vk::MemoryPropertyFlagBits::eDeviceLocal)
                            .withVmaUsage(VMA_MEMORY_USAGE_AUTO)
                            .buildUnique(_context);
    upload_buffer_data_immediate(_upload_ctx, mesh.indices.data(), sizeof(uint32_t) * mesh.indices.size(), *gpu_mesh->
                                 index_buffer, 0, BufferUsage::IndexBuffer);

    gpu_mesh->vertex_count = vertex_count;
    gpu_mesh->index_count  = index_count;
    return gpu_mesh;
}

std::shared_ptr<GPUTexture> GpuResourceManager::getDefaultWhiteTexture()
{
    if (_default_white_texture)
    {
        return _default_white_texture;
    }

    TextureData texture{};
    texture.width    = 1;
    texture.height   = 1;
    texture.depth    = 1;
    texture.channels = 4;
    texture.pixels   = {255, 255, 255, 255};

    _default_white_texture = uploadTextureData(texture);
    return _default_white_texture;
}

void GpuResourceManager::ensureMaterialDescriptorSet(GPUMaterial& material)
{
    if (!_material_descriptor_layout || !_material_descriptor_allocator)
    {
        return;
    }

    if (material.descriptor_set)
    {
        return;
    }

    vk::DescriptorSet descriptor_set = _material_descriptor_allocator->allocate(*_material_descriptor_layout);

    auto texture = material.base_color_tex ? material.base_color_tex : getDefaultWhiteTexture();
    vk::DescriptorImageInfo image_info = vkcore::makeCombinedImageInfo(
        texture->view->getHandle(),
        texture->sampler->getHandle(),
        texture->layout);

    vkcore::DescriptorSetWriter writer;
    writer.writeImage(0, vk::DescriptorType::eCombinedImageSampler, image_info, 0);
    writer.update(_context, descriptor_set);

    material.descriptor_set = descriptor_set;
}

std::shared_ptr<GPUTexture> GpuResourceManager::uploadTextureData(const TextureData& texture)
{
    const vk::DeviceSize image_size = texture.width * texture.height * texture.depth * 4;

    ImageResource::Builder image_builder;
    auto                   image = image_builder.setFormat(vk::Format::eR8G8B8A8Unorm)
                                                .setExtent({texture.width, texture.height, 1})
                                                .setUsage(
                                                     vk::ImageUsageFlagBits::eTransferDst |
                                                     vk::ImageUsageFlagBits::eSampled)
                                                .buildUnique(_context);

    upload_image_data_immediate(_upload_ctx, texture.pixels.data(), image_size, *image, ImageUsage::Sampled);

    auto view    = ImageViewResource::create2D(_context, *image, vk::Format::eR8G8B8A8Unorm);
    auto sampler = std::make_unique<SamplerResource>(_context, makeLinearRepeatSamplerInfo());

    auto gpu_tex     = std::make_shared<GPUTexture>();
    gpu_tex->image   = std::move(image);
    gpu_tex->view    = std::move(view);
    gpu_tex->sampler = std::move(sampler);
    gpu_tex->layout  = vk::ImageLayout::eShaderReadOnlyOptimal;
    return gpu_tex;
}

std::shared_ptr<GPUMesh> GpuResourceManager::loadMesh(UUID id)
{
    return _cache.resolve<GPUMesh>(id, [&]
    {
        auto cpu = _cpu_loader.load<MeshData>(id);
        if (!cpu) return std::shared_ptr<GPUMesh>{};
        return uploadMeshData(*cpu);
    });
}

std::shared_ptr<GPUTexture> GpuResourceManager::loadTexture(UUID id)
{
    return _cache.resolve<GPUTexture>(id, [&]
    {
        auto cpu = _cpu_loader.load<TextureData>(id);
        if (!cpu) return std::shared_ptr<GPUTexture>{};
        return uploadTextureData(*cpu);
    });
}

std::shared_ptr<GPUMaterial> GpuResourceManager::loadMaterial(UUID id)
{
    return _cache.resolve<GPUMaterial>(id, [&]
    {
        auto cpu = _cpu_loader.load<MaterialData>(id);
        if (!cpu)
        {
            return getDefaultMaterial();
        }

        auto gpu        = std::make_shared<GPUMaterial>();
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
        ensureMaterialDescriptorSet(*gpu);
        return gpu;
    });
}
} // namespace dk
