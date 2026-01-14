#include "OpaquePass.h"

#include "vk_engine.h"
#include "render graph/RenderGraph.h"

namespace dk {
OpaquePass::OpaquePass(VulkanEngine& engine)
    : _engine(engine)
{
}

void OpaquePass::registerToGraph(RenderGraph& graph)
{
    graph.addTask<OpaquePassData>(
        "Opaque Pass",
        [this](OpaquePassData&, RenderTaskBuilder&)
        {
        },
        [this](const OpaquePassData&, RenderGraphContext& ctx)
        {
            this->record(ctx);
        }
    );
}

void OpaquePass::record(RenderGraphContext& ctx) const
{
    if (!_draw_lists || !_frame_context)
    {
        return;
    }

    _engine.record_opaque_pass(ctx, *_frame_context, *_draw_lists);
}
} // namespace dk
