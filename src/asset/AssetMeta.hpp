// src/asset/AssetNode.hpp
#pragma once
#include <memory>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

#include "UUID.hpp"

namespace dk {
struct AssetMeta
{
    UUID              uuid;
    std::string       importer;        // "gltf","png"…
    nlohmann::json    importOpts;
    std::vector<UUID> dependencies;    // 子 Raw uuid
    uint64_t          contentHash = 0; // 增量导入
    std::string       rawPath;         // e.g. "47af.rawmesh"
};
}
