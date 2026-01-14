#pragma once

#include <memory>
#include <unordered_map>

#include "Scene/Bound.h"
#include "UUID.hpp"
#include "vk_types.h"
#include "vk_descriptors.h"

namespace dk {
class VulkanEngine;
class ResourceLoader;
struct MeshData;
struct MaterialData;
struct TextureData;

class GpuResourceCache
{
public:
    struct MeshEntry
    {
        GPUMeshBuffers buffers{};
        Bounds         local_bounds{};
        uint32_t       index_count{0};
    };

    struct MaterialEntry
    {
        MaterialInstance material{};
        AllocatedBuffer  constants_buffer{};
    };

    GpuResourceCache(VulkanEngine& engine, ResourceLoader& loader);
    ~GpuResourceCache();

    std::shared_ptr<MeshEntry> ensureMesh(const UUID& id);
    std::shared_ptr<MaterialEntry> ensureMaterial(const UUID& id);
    std::shared_ptr<MaterialEntry> defaultMaterial();

private:
    std::shared_ptr<MeshEntry> createMesh(const MeshData& mesh);
    std::shared_ptr<MaterialEntry> createMaterial(const MaterialData& material);
    AllocatedImage createTexture(const TextureData& texture);

    VulkanEngine&  _engine;
    ResourceLoader& _loader;

    DescriptorAllocatorGrowable _descriptor_pool;

    std::unordered_map<UUID, std::shared_ptr<MeshEntry>> _meshes;
    std::unordered_map<UUID, std::shared_ptr<MaterialEntry>> _materials;
    std::unordered_map<UUID, AllocatedImage> _textures;

    std::shared_ptr<MaterialEntry> _default_material;
};
} // namespace dk
