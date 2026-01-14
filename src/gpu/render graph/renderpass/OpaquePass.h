#pragma once

#include "render graph/Resource.h"

namespace dk {
class RenderGraph;
class VulkanEngine;
struct DrawLists;
struct FrameContext;
struct ImageDesc;
class FrameGraphImage;

template <typename Desc, typename Actual>
class RGResource;

class OpaquePass
{
public:
    explicit OpaquePass(VulkanEngine& engine);

    void setDrawLists(const DrawLists* lists) { _draw_lists = lists; }
    void setFrameContext(const FrameContext* context) { _frame_context = context; }

    void registerToGraph(RenderGraph& graph);

private:
    struct OpaquePassData
    {
    };

    void record(RenderGraphContext& ctx) const;

    VulkanEngine&     _engine;
    const DrawLists*  _draw_lists{nullptr};
    const FrameContext* _frame_context{nullptr};
};
} // namespace dk
