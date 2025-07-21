// src/asset/AssetNode.hpp
#pragma once
#include <memory>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <tsl/robin_map.h>

#include "UUID.hpp"

namespace dk {
struct AssetMeta
{
    UUID              uuid;
    std::string       importer;        // "gltf","png"…
    nlohmann::json    import_opts;
    tsl::robin_map<std::string, UUID> dependencies;    // 子 Raw uuid
    uint64_t          content_hash = 0; // 增量导入
    std::string       raw_path;         // e.g. "47af.rawmesh"
};
}
