#include "RenderWorld.h"

#include <stack>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "scene/SceneTypes.h"
#include "resource/cpu/MeshLoader.h"

namespace dk::render {
namespace {
glm::mat4 to_matrix(const Transform& t)
{
    glm::mat4 translation = glm::translate(glm::mat4(1.0f), t.translation);
    glm::mat4 rotation    = glm::mat4_cast(t.rotation);
    glm::mat4 scale       = glm::scale(glm::mat4(1.0f), t.scale);
    return translation * rotation * scale;
}
} // namespace

void RenderWorld::extractFromScene(const Scene& scene, ResourceLoader& loader, GpuResourceManager& gpu_cache)
{
    _proxies.clear();
    _node_map.clear();

    const auto root = scene.getRoot();
    if (!root)
    {
        return;
    }

    std::stack<std::pair<std::shared_ptr<SceneNode>, glm::mat4>> stack;
    stack.push({root, glm::mat4(1.0f)});

    while (!stack.empty())
    {
        auto [node, parent_transform] = stack.top();
        stack.pop();

        glm::mat4 world = parent_transform * to_matrix(node->local_transform);

        if (auto* mesh_component = node->getComponent<MeshInstanceComponent>())
        {
            auto cpu_mesh = loader.load<MeshData>(mesh_component->mesh_asset);
            if (cpu_mesh)
            {
                RenderProxy proxy{};
                proxy.node_id         = node->id;
                proxy.world_transform = world;
                proxy.mesh_asset      = mesh_component->mesh_asset;
                proxy.material_asset  = mesh_component->material_asset;
                proxy.gpu_mesh        = gpu_cache.loadMesh(mesh_component->mesh_asset);
                proxy.gpu_material    = gpu_cache.loadMaterial(mesh_component->material_asset);

                const AABB local_bounds = getMeshBounds(mesh_component->mesh_asset, *cpu_mesh);
                proxy.world_bounds      = local_bounds.transform(world);

                _node_map[node->id] = _proxies.size();
                _proxies.push_back(std::move(proxy));
            }
        }

        for (const auto& child : node->children)
        {
            if (child)
            {
                stack.push({child, world});
            }
        }
    }
}

AABB RenderWorld::getMeshBounds(const UUID& mesh_id, const MeshData& mesh)
{
    if (auto it = _mesh_bounds_cache.find(mesh_id); it != _mesh_bounds_cache.end())
    {
        return it->second;
    }

    AABB bounds;
    for (const auto& pos : mesh.positions)
    {
        bounds.expand(pos);
    }
    _mesh_bounds_cache.emplace(mesh_id, bounds);
    return bounds;
}
} // namespace dk::render
