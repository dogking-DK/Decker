#include "RenderWorld.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include "Scene.h"
#include "SceneTypes.h"

namespace dk {
namespace {
    glm::mat4 to_matrix(const Transform& transform)
    {
        glm::mat4 translation = glm::translate(glm::mat4(1.0f), transform.translation);
        glm::mat4 rotation    = glm::mat4_cast(transform.rotation);
        glm::mat4 scale       = glm::scale(glm::mat4(1.0f), transform.scale);
        return translation * rotation * scale;
    }
}

void RenderWorld::extractFromScene(const Scene& scene)
{
    clear();

    auto root = scene.getRoot();
    if (!root)
    {
        return;
    }

    extractNode(root, glm::mat4(1.0f));
}

void RenderWorld::clear()
{
    _proxies.clear();
    _node_to_index.clear();
}

void RenderWorld::extractNode(const std::shared_ptr<SceneNode>& node, const glm::mat4& parent_transform)
{
    if (!node)
    {
        return;
    }

    glm::mat4 world_transform = parent_transform * to_matrix(node->local_transform);

    if (auto* mesh_component = node->getComponent<MeshInstanceComponent>())
    {
        RenderProxy proxy{};
        proxy.node_id         = node->id;
        proxy.mesh_asset      = mesh_component->mesh_asset;
        proxy.material_asset  = mesh_component->material_asset;
        proxy.world_transform = world_transform;

        _node_to_index[proxy.node_id] = _proxies.size();
        _proxies.push_back(std::move(proxy));
    }

    for (const auto& child : node->children)
    {
        extractNode(child, world_transform);
    }
}
} // namespace dk
