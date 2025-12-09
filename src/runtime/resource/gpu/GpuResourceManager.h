#pragma once

#include <memory>

#include "CommandPool.h"
#include "GPUMaterial.h"
#include "GPUMesh.h"
#include "GPUTexture.h"
#include "ResourceCache.h"
#include "ResourceLoader.h"

namespace dk {
class GpuResourceManager
{
public:
    GpuResourceManager(vkcore::VulkanContext& context, vkcore::CommandPool& command_pool, ResourceLoader& cpu_loader);

    std::shared_ptr<GPUMesh>     loadMesh(UUID id);
    std::shared_ptr<GPUTexture>  loadTexture(UUID id);
    std::shared_ptr<GPUMaterial> loadMaterial(UUID id);

private:
    std::shared_ptr<GPUMesh> uploadMeshData(const MeshData& mesh);
    std::shared_ptr<GPUTexture> uploadTextureData(const TextureData& texture);

    vkcore::VulkanContext& _context;
    vkcore::CommandPool&   _command_pool;
    ResourceLoader&        _cpu_loader;
    ResourceCache          _cache;
};
} // namespace dk
