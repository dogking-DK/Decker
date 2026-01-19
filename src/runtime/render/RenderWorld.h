#pragma once

#include <unordered_map>
#include <vector>

#include <glm/mat4x4.hpp>

#include "UUID.hpp"
#include "Scene.h"
#include "resource/cpu/ResourceLoader.h"
#include "resource/gpu/GpuResourceManager.h"
#include "RenderTypes.h"
#include "BVH/AABB.hpp"

namespace dk::render {
struct RenderProxy
{
    UUID                              node_id{};
    glm::mat4                         world_transform{1.0f};
    AABB                              world_bounds{};
    UUID                              mesh_asset{};
    UUID                              material_asset{};
    std::shared_ptr<GPUMesh>          gpu_mesh{};
    std::shared_ptr<GPUMaterial>      gpu_material{};
    bool                              visible{true};
};

class RenderWorld
{
public:
    void extractFromScene(const Scene& scene, ResourceLoader& loader, GpuResourceManager& gpu_cache);

    const std::vector<RenderProxy>& proxies() const { return _proxies; }
    std::vector<RenderProxy>&       proxies() { return _proxies; }
    AABB getAllBound() const;
private:
    AABB getMeshBounds(const UUID& mesh_id, const MeshData& mesh);

    std::vector<RenderProxy>               _proxies;
    std::unordered_map<UUID, size_t>       _node_map;
    std::unordered_map<UUID, AABB>         _mesh_bounds_cache;
};
} // namespace dk::render
