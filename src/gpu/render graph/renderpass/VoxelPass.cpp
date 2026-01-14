#include "VoxelPass.h"

#include "render graph/RenderGraph.h"

namespace dk {
void VoxelPass::registerToGraph(RenderGraph& graph)
{
    graph.addTask<VoxelPassData>(
        "Voxel Pass",
        [this](VoxelPassData&, RenderTaskBuilder&)
        {
        },
        [this](const VoxelPassData&, RenderGraphContext& ctx)
        {
            this->record(ctx);
        }
    );
}

void VoxelPass::record(RenderGraphContext& ctx) const
{
    (void)ctx;
    if (!_data)
    {
        return;
    }
    // TODO: 若有 surface_mesh，使用网格管线绘制；否则体素 raymarch。
    // 需要接入 _data 的纹理/参数资源，并与 RenderGraph 管线资源建立依赖。
}
} // namespace dk
