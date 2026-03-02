#pragma once

#include <unordered_map>
#include <string>
#include <vector>

#include "GraphAsset.h"

namespace dk {
class RenderPassRegistry;

struct CompiledPinSource
{
    GraphNodeId sourceNode{0};
    std::string sourcePin;
};

struct CompiledGraphNode
{
    GraphNodeId id{0};
    std::string type;
    std::unordered_map<std::string, CompiledPinSource> inputPins;
};

struct CompiledGraphAsset
{
    std::string                 name;
    std::uint32_t               schemaVersion{1};
    std::vector<GraphNodeId>    executionOrder;
    std::vector<CompiledGraphNode> nodes;
    std::vector<GraphResourceDesc> resources;
};

bool compileGraphAsset(const GraphAsset& graph,
                       CompiledGraphAsset& out_compiled,
                       std::string& out_error,
                       const RenderPassRegistry* pass_registry = nullptr);

}
