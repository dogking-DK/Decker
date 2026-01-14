#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#include <glm/mat4x4.hpp>

#include "UUID.hpp"

namespace dk {
class Scene;
struct SceneNode;

struct RenderProxy
{
    UUID      node_id{};
    UUID      mesh_asset{};
    UUID      material_asset{};
    glm::mat4 world_transform{1.0f};
    uint32_t  flags{0};
};

class RenderWorld
{
public:
    void extractFromScene(const Scene& scene);
    void clear();

    const std::vector<RenderProxy>& proxies() const { return _proxies; }
    const std::unordered_map<UUID, size_t>& nodeIndex() const { return _node_to_index; }

private:
    void extractNode(const std::shared_ptr<SceneNode>& node, const glm::mat4& parent_transform);

    std::vector<RenderProxy>           _proxies;
    std::unordered_map<UUID, size_t>   _node_to_index;
};
} // namespace dk
