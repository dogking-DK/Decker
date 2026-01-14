#include "FluidVolumePass.h"

#include "render graph/RenderGraph.h"

namespace dk {
void FluidVolumePass::registerToGraph(RenderGraph& graph)
{
    graph.addTask<FluidPassData>(
        "Fluid Volume Pass",
        [this](FluidPassData&, RenderTaskBuilder&)
        {
        },
        [this](const FluidPassData&, RenderGraphContext& ctx)
        {
            this->record(ctx);
        }
    );
}

void FluidVolumePass::record(RenderGraphContext& ctx) const
{
    (void)ctx;
    if (!_data)
    {
        return;
    }
    // TODO: 接入流体体积渲染（raymarch/voxel cone tracing）。
    // 需要使用 _data 提供的纹理/参数并写入 RenderGraph 中对应的颜色/深度资源。
}
} // namespace dk
