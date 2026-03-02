#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "Resource.h"

namespace dk {

using GraphNodeId = std::uint32_t;
using GraphEdgeId = std::uint32_t;

enum class QueueClass : std::uint8_t
{
    Graphics,
    Compute,
    Transfer,
};

enum class ParamType : std::uint8_t
{
    Bool,
    Int,
    Float,
    Vec2,
    Vec3,
    Vec4,
    String,
};

using ParamValue = std::variant<
    bool,
    std::int32_t,
    float,
    std::array<float, 2>,
    std::array<float, 3>,
    std::array<float, 4>,
    std::string>;

struct GraphParam
{
    std::string name;
    ParamType   type{ParamType::Float};
    ParamValue  value{0.0f};
};

struct GraphImageDesc
{
    std::uint32_t width{1};
    std::uint32_t height{1};
    std::uint32_t depth{1};
    std::uint32_t mipLevels{1};
    std::uint32_t arrayLayers{1};
    std::uint32_t format{0};
    std::uint32_t usage{0};
    std::uint32_t samples{1};
    std::uint32_t aspectMask{0};
};

struct GraphBufferDesc
{
    std::uint64_t size{0};
    std::uint32_t usage{0};
};

using GraphResourceDescPayload = std::variant<GraphImageDesc, GraphBufferDesc>;

struct GraphResourceDesc
{
    std::string              name;
    ResourceKind             kind{ResourceKind::Unknown};
    ResourceLifetime         lifetime{ResourceLifetime::Transient};
    std::string              externalKey;
    GraphResourceDescPayload payload{};
};

struct GraphNodeAsset
{
    GraphNodeId              id{0};
    std::string              type;
    std::vector<GraphParam>  params;
};

struct GraphEdgeAsset
{
    GraphEdgeId   id{0};
    GraphNodeId   fromNode{0};
    std::string   fromPin;
    GraphNodeId   toNode{0};
    std::string   toPin;
};

struct GraphAsset
{
    std::uint32_t                    schemaVersion{1};
    std::string                      name;
    std::vector<GraphResourceDesc>   resources;
    std::vector<GraphNodeAsset>      nodes;
    std::vector<GraphEdgeAsset>      edges;
};

}

