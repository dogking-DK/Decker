#pragma once

#include <string>
#include <vector>

#include "GraphAsset.h"

namespace dk {

struct CompiledGraphAsset
{
    std::string                 name;
    std::uint32_t               schemaVersion{1};
    std::vector<GraphNodeId>    executionOrder;
    std::vector<GraphResourceDesc> resources;
};

bool compileGraphAsset(const GraphAsset& graph, CompiledGraphAsset& out_compiled, std::string& out_error);

}

