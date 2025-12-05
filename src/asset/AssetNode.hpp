// src/asset/AssetNode.hpp
#pragma once
#include <memory>
#include <string>
#include <vector>
#include "UUID.hpp"

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

struct AssetNode
{
    UUID                                    id;
    AssetKind                               kind;
    std::string                             name;
    std::vector<std::shared_ptr<AssetNode>> children;
};
}
