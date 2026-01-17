#pragma once

#include <memory>

#include "GPUMaterial.h"
#include "GPUMesh.h"
#include "GPUTexture.h"
#include "UUID.hpp"
#include "resource/cpu/ResourceCache.h"

namespace dk {
struct TextureData;
struct MeshData;
class ResourceLoader;

namespace vkcore {
    class CommandPool;
    class VulkanContext;
    class DescriptorSetLayout;
    class GrowableDescriptorAllocator;
}

class GpuResourceManager
{
public:
    GpuResourceManager(vkcore::VulkanContext& ctx, vkcore::UploadContext& upload_ctx, ResourceLoader& cpu_loader);
    ~GpuResourceManager();
    void setMaterialDescriptorLayout(vkcore::DescriptorSetLayout* layout);
    std::shared_ptr<GPUMaterial> getDefaultMaterial();
    std::shared_ptr<GPUMesh>     loadMesh(UUID id);
    std::shared_ptr<GPUTexture>  loadTexture(UUID id);
    std::shared_ptr<GPUMaterial> loadMaterial(UUID id);

private:
    std::shared_ptr<GPUMesh> uploadMeshData(const MeshData& mesh);
    std::shared_ptr<GPUTexture> uploadTextureData(const TextureData& texture);
    std::shared_ptr<GPUTexture> getDefaultWhiteTexture();
    void                        ensureMaterialDescriptorSet(GPUMaterial& material);

    vkcore::VulkanContext& _context;
    vkcore::UploadContext& _upload_ctx;
    ResourceLoader&        _cpu_loader;
    ResourceCache          _cache;
    vkcore::DescriptorSetLayout*                   _material_descriptor_layout{nullptr};
    std::unique_ptr<vkcore::GrowableDescriptorAllocator> _material_descriptor_allocator{nullptr};
    std::shared_ptr<GPUTexture>                    _default_white_texture{};
    std::shared_ptr<GPUMaterial>                   _default_material{};
};
} // namespace dk
