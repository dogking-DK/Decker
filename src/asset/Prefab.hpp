#pragma once

#include <optional>
#include <vector>

#include <nlohmann/json.hpp>

#include "AssetNode.hpp"

namespace dk {
struct PrefabNode
{
    UUID                id{};
    AssetKind           kind{AssetKind::Node};
    std::string         name;
    std::vector<PrefabNode> children;
};

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
    j = nlohmann::json{{"id", to_string(node.id)}, {"kind", to_string(node.kind)}, {"name", node.name},
                       {"children", node.children}};
}

inline void from_json(const nlohmann::json& j, PrefabNode& node)
{
    node.id   = uuid_from_string(j.at("id").get<std::string>());
    node.name = j.value("name", "");
    if (auto kind = asset_kind_from_string(j.at("kind").get<std::string>()); kind.has_value())
        node.kind = *kind;
    if (j.contains("children")) j.at("children").get_to(node.children);
}

inline PrefabNode prefab_from_asset_node(const AssetNode& src)
{
    PrefabNode prefab{};
    prefab.id   = src.id;
    prefab.kind = src.kind;
    prefab.name = src.name;

    for (const auto& child : src.children)
    {
        if (child) prefab.children.push_back(prefab_from_asset_node(*child));
    }
    return prefab;
}
} // namespace dk
