#pragma once
#include "render graph/Resource.h"

namespace dk {
struct FrameGraphImage;
struct ImageDesc;
class RenderGraph;

namespace vkcore {
    class VulkanContext;
}

class BlitPass
{
public:

    void init(vkcore::VulkanContext& ctx);  // 创建 shader module / pipeline / layout / sampler 等
    void destroy(vkcore::VulkanContext& ctx);

    // 把 FXAA 作为一个 Task 注册进 RenderGraph
    void registerToGraph(
        dk::RenderGraph&                                  graph,
        Resource<dk::ImageDesc, dk::FrameGraphImage>*     sceneColor,
        dk::Resource<dk::ImageDesc, dk::FrameGraphImage>* swapchainColor
    );
    void record(RenderGraphContext& ctx, FxaaPassData const& data);
private:

};

} // namespace dk::rg
