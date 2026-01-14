#pragma once

#include "render graph/Resource.h"
#include "render/RenderData.h"

namespace dk {
class RenderGraph;

class FluidVolumePass
{
public:
    void setData(const FluidRenderData* data) { _data = data; }
    void registerToGraph(RenderGraph& graph);

private:
    struct FluidPassData
    {
    };

    void record(RenderGraphContext& ctx) const;

    const FluidRenderData* _data{nullptr};
};
} // namespace dk
