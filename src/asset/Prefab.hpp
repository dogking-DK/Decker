#pragma once

#include <optional>
#include <vector>
#include "UUID.hpp"
#include <nlohmann/json.hpp>

#include "Transform.h"

namespace dk {
enum class AssetKind : uint8_t
{
    Scene,
    Node,
    Mesh,
    Material,
    Image,
    Primitive,
};

struct PrefabNode
{
    UUID                    id{};
    AssetKind               kind{AssetKind::Node};
    std::string             name;
    Transform               local_transform{};
    std::vector<PrefabNode> children;
};

inline void to_json(nlohmann::json& j, const Transform& transform)
{
    j = nlohmann::json{
        {"translation", {transform.translation.x, transform.translation.y, transform.translation.z}},
        {"rotation", {transform.rotation.x, transform.rotation.y, transform.rotation.z, transform.rotation.w}},
        {"scale", {transform.scale.x, transform.scale.y, transform.scale.z}},
    };
}

inline void from_json(const nlohmann::json& j, Transform& transform)
{
    if (j.contains("translation"))
    {
        const auto& t     = j.at("translation");
        transform.translation = {t.at(0).get<float>(), t.at(1).get<float>(), t.at(2).get<float>()};
    }
    if (j.contains("rotation"))
    {
        const auto& r    = j.at("rotation");
        transform.rotation = {r.at(3).get<float>(), r.at(0).get<float>(), r.at(1).get<float>(),
                              r.at(2).get<float>()};
    }
    if (j.contains("scale"))
    {
        const auto& s = j.at("scale");
        transform.scale = {s.at(0).get<float>(), s.at(1).get<float>(), s.at(2).get<float>()};
    }
}

inline std::string to_string(const AssetKind kind)
{
    switch (kind)
    {
    case AssetKind::Scene: return "Scene";
    case AssetKind::Node: return "Node";
    case AssetKind::Mesh: return "Mesh";
    case AssetKind::Material: return "Material";
    case AssetKind::Image: return "Image";
    case AssetKind::Primitive: return "Primitive";
    }
    return "Unknown";
}

inline std::optional<AssetKind> asset_kind_from_string(const std::string& kind)
{
    if (kind == "Scene") return AssetKind::Scene;
    if (kind == "Node") return AssetKind::Node;
    if (kind == "Mesh") return AssetKind::Mesh;
    if (kind == "Material") return AssetKind::Material;
    if (kind == "Image") return AssetKind::Image;
    if (kind == "Primitive") return AssetKind::Primitive;
    return std::nullopt;
}

inline void to_json(nlohmann::json& j, const PrefabNode& node)
{
    j = nlohmann::json{
        {"id", to_string(node.id)}, {"kind", to_string(node.kind)}, {"name", node.name},
        {"children", node.children}, {"transform", node.local_transform}
    };
}

inline void from_json(const nlohmann::json& j, PrefabNode& node)
{
    node.id   = uuid_from_string(j.at("id").get<std::string>());
    node.name = j.value("name", "");
    if (auto kind = asset_kind_from_string(j.at("kind").get<std::string>()); kind.has_value())
        node.kind = *kind;
    if (j.contains("transform")) j.at("transform").get_to(node.local_transform);
    if (j.contains("children")) j.at("children").get_to(node.children);
}
} // namespace dk
