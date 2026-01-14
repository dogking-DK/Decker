#include "GpuResourceCache.h"

#include <algorithm>
#include <cstring>

#include <glm/vec4.hpp>

#include "vk_engine.h"
#include "UUID.hpp"
#include "resource/cpu/MaterialLoader.h"
#include "resource/cpu/MeshLoader.h"
#include "resource/cpu/ResourceLoader.h"
#include "resource/cpu/TextureLoader.h"

namespace dk {
namespace {
    Bounds compute_bounds(const MeshData& mesh)
    {
        Bounds bounds;
        bounds.reset();
        if (mesh.positions.empty())
        {
            return bounds;
        }

        glm::vec3 min_edge = mesh.positions.front();
        glm::vec3 max_edge = mesh.positions.front();
        for (const auto& pos : mesh.positions)
        {
            min_edge = glm::min(min_edge, pos);
            max_edge = glm::max(max_edge, pos);
        }

        bounds.min_edge      = min_edge;
        bounds.max_edge      = max_edge;
        bounds.origin        = (max_edge + min_edge) * 0.5f;
        bounds.extents       = (max_edge - min_edge) * 0.5f;
        bounds.sphere_radius = glm::length(bounds.extents);
        return bounds;
    }

    Vertex to_vertex(const MeshData& mesh, uint32_t index)
    {
        Vertex v{};
        if (index < mesh.positions.size())
        {
            v.position = mesh.positions[index];
        }
        v.normal = (index < mesh.normals.size()) ? mesh.normals[index] : glm::vec3{0.0f, 1.0f, 0.0f};
        if (!mesh.texcoords.empty() && index < mesh.texcoords.front().size())
        {
            v.uv_x = mesh.texcoords.front()[index].x;
            v.uv_y = mesh.texcoords.front()[index].y;
        }
        else
        {
            v.uv_x = 0.0f;
            v.uv_y = 0.0f;
        }
        v.color = glm::vec4(1.0f);
        return v;
    }
}

GpuResourceCache::GpuResourceCache(VulkanEngine& engine, ResourceLoader& loader)
    : _engine(engine), _loader(loader)
{
    std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 4.0f},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4.0f},
    };
    _descriptor_pool.init(_engine._context->getDevice(), 64, sizes);
}

GpuResourceCache::~GpuResourceCache()
{
    auto device = _engine._context->getDevice();
    _descriptor_pool.destroy_pools(device);

    for (auto& [_, material] : _materials)
    {
        if (material && material->constants_buffer.buffer != VK_NULL_HANDLE)
        {
            _engine.destroy_buffer(material->constants_buffer);
        }
    }

    for (auto& [_, mesh] : _meshes)
    {
        if (!mesh)
        {
            continue;
        }
        if (mesh->buffers.vertexBuffer.buffer != VK_NULL_HANDLE)
        {
            _engine.destroy_buffer(mesh->buffers.vertexBuffer);
        }
        if (mesh->buffers.indexBuffer.buffer != VK_NULL_HANDLE)
        {
            _engine.destroy_buffer(mesh->buffers.indexBuffer);
        }
    }

    for (auto& [_, texture] : _textures)
    {
        if (texture.image != VK_NULL_HANDLE)
        {
            _engine.destroy_image(texture);
        }
    }
}

std::shared_ptr<GpuResourceCache::MeshEntry> GpuResourceCache::ensureMesh(const UUID& id)
{
    if (id.is_nil())
    {
        return {};
    }

    auto it = _meshes.find(id);
    if (it != _meshes.end())
    {
        return it->second;
    }

    auto cpu_mesh = _loader.load<MeshData>(id);
    if (!cpu_mesh)
    {
        return {};
    }

    auto entry = createMesh(*cpu_mesh);
    _meshes.emplace(id, entry);
    return entry;
}

std::shared_ptr<GpuResourceCache::MaterialEntry> GpuResourceCache::ensureMaterial(const UUID& id)
{
    if (id.is_nil())
    {
        return defaultMaterial();
    }

    auto it = _materials.find(id);
    if (it != _materials.end())
    {
        return it->second;
    }

    auto cpu_material = _loader.load<MaterialData>(id);
    if (!cpu_material)
    {
        return defaultMaterial();
    }

    auto entry = createMaterial(*cpu_material);
    _materials.emplace(id, entry);
    return entry;
}

std::shared_ptr<GpuResourceCache::MaterialEntry> GpuResourceCache::defaultMaterial()
{
    if (_default_material)
    {
        return _default_material;
    }

    MaterialData fallback{};
    fallback.base_color = {255, 255, 255, 255};
    fallback.metallic   = 0.0f;
    fallback.roughness  = 1.0f;
    _default_material   = createMaterial(fallback);
    return _default_material;
}

std::shared_ptr<GpuResourceCache::MeshEntry> GpuResourceCache::createMesh(const MeshData& mesh)
{
    std::vector<Vertex> vertices;
    const uint32_t vertex_count = mesh.vertex_count > 0
                                      ? mesh.vertex_count
                                      : static_cast<uint32_t>(mesh.positions.size());
    vertices.resize(vertex_count);
    for (uint32_t i = 0; i < vertex_count; ++i)
    {
        vertices[i] = to_vertex(mesh, i);
    }

    auto entry = std::make_shared<MeshEntry>();
    entry->buffers     = _engine.uploadMesh(mesh.indices, vertices);
    entry->local_bounds = compute_bounds(mesh);
    entry->index_count = static_cast<uint32_t>(mesh.indices.size());
    return entry;
}

std::shared_ptr<GpuResourceCache::MaterialEntry> GpuResourceCache::createMaterial(const MaterialData& material)
{
    auto entry = std::make_shared<MaterialEntry>();

    GLTFMetallic_Roughness::MaterialConstants constants{};
    constants.colorFactors = glm::vec4(
        material.base_color[0] / 255.0f,
        material.base_color[1] / 255.0f,
        material.base_color[2] / 255.0f,
        material.base_color[3] / 255.0f);
    constants.metal_rough_factors = glm::vec4(material.metallic, material.roughness, 0.0f, 0.0f);

    auto use_texture = [&](const std::shared_ptr<TextureData>& tex) -> uint32_t
    {
        if (!tex)
        {
            return _engine.texCache.AddTexture(_engine._whiteImage.imageView, _engine._defaultSamplerLinear).Index;
        }
        auto texture = createTexture(*tex);
        auto id      = uuid_generate();
        _textures.emplace(id, texture);
        return _engine.texCache.AddTexture(texture.imageView, _engine._defaultSamplerLinear).Index;
    };

    constants.colorTexID      = use_texture(material.base_color_tex);
    constants.metalRoughTexID = use_texture(material.metal_rough_tex);
    constants.normalTexID     = use_texture(material.normal_tex);

    entry->constants_buffer = _engine.create_buffer(sizeof(constants), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                    VMA_MEMORY_USAGE_CPU_TO_GPU);
    std::memcpy(entry->constants_buffer.info.pMappedData, &constants, sizeof(constants));

    GLTFMetallic_Roughness::MaterialResources resources{};
    resources.dataBuffer       = entry->constants_buffer.buffer;
    resources.dataBufferOffset = 0;

    entry->material = _engine.metalRoughMaterial.write_material(_engine._context->getDevice(), MaterialPass::MainColor,
                                                                resources, _descriptor_pool);
    return entry;
}

AllocatedImage GpuResourceCache::createTexture(const TextureData& texture)
{
    VkExtent3D extent{texture.width, texture.height, texture.depth};
    return _engine.create_image(const_cast<uint8_t*>(texture.pixels.data()), extent, VK_FORMAT_R8G8B8A8_UNORM,
                                VK_IMAGE_USAGE_SAMPLED_BIT);
}
} // namespace dk
