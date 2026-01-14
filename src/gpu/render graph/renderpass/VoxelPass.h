#pragma once

#include "render graph/Resource.h"
#include "render/RenderData.h"

namespace dk {
class RenderGraph;

class VoxelPass
{
public:
    void setData(const VoxelRenderData* data) { _data = data; }
    void registerToGraph(RenderGraph& graph);

private:
    struct VoxelPassData
    {
    };

    void record(RenderGraphContext& ctx) const;

    const VoxelRenderData* _data{nullptr};
};
} // namespace dk
