// src/asset/AssetNode.hpp
#pragma once
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <tsl/robin_map.h>

#include "UUID.hpp"

namespace dk {

enum class AssetDependencyType : uint8_t
{
    Material,
    BaseColorTexture,
    MetallicRoughnessTexture,
    NormalTexture,
    OcclusionTexture,
    EmissiveTexture,
};

struct DependencyHash
{
    std::size_t operator()(AssetDependencyType t) const noexcept
    {
        return static_cast<std::size_t>(t);
    }
};

inline std::string to_string(AssetDependencyType type)
{
    switch (type)
    {
    case AssetDependencyType::Material: return "material";
    case AssetDependencyType::BaseColorTexture: return "baseColorTexture";
    case AssetDependencyType::MetallicRoughnessTexture: return "metallicRoughnessTexture";
    case AssetDependencyType::NormalTexture: return "normalTexture";
    case AssetDependencyType::OcclusionTexture: return "occlusionTexture";
    case AssetDependencyType::EmissiveTexture: return "emissiveTexture";
    }
    return "";
}

inline std::optional<AssetDependencyType> dependency_from_string(const std::string& key)
{
    if (key == "material") return AssetDependencyType::Material;
    if (key == "baseColorTexture") return AssetDependencyType::BaseColorTexture;
    if (key == "metallicRoughnessTexture") return AssetDependencyType::MetallicRoughnessTexture;
    if (key == "normalTexture") return AssetDependencyType::NormalTexture;
    if (key == "occlusionTexture") return AssetDependencyType::OcclusionTexture;
    if (key == "emissiveTexture") return AssetDependencyType::EmissiveTexture;
    return std::nullopt;
}

using DependencyMap = tsl::robin_map<AssetDependencyType, UUID, DependencyHash>;

struct AssetMeta
{
    UUID           uuid;
    std::string    importer;        // "gltf","png"…
    nlohmann::json import_opts;
    DependencyMap  dependencies;    // 子 Raw uuid
    uint64_t       content_hash = 0; // 增量导入
    std::string    raw_path;         // e.g. "47af.rawmesh"
};
}
