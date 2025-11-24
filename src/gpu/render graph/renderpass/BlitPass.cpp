#include "BlitPass.h"

#include "render graph/RenderGraph.h"
#include "render graph/RenderTaskBuilder.h"

void dk::BlitPass::registerToGraph(dk::RenderGraph& graph, Resource<dk::ImageDesc, dk::FrameGraphImage>* sceneColor,
                                   dk::Resource<dk::ImageDesc, dk::FrameGraphImage>* swapchainColor)
{
    _sceneRes = sceneColor;
    _swapRes = swapchainColor;

    struct FxaaPassData {
        dk::Resource<dk::ImageDesc, dk::FrameGraphImage>* src;
        dk::Resource<dk::ImageDesc, dk::FrameGraphImage>* dst;
    };

    graph.addTask<FxaaPassData>(
        "FXAA",
        // setup：声明资源读写 + attachment
        [this](FxaaPassData& data, dk::RenderTaskBuilder& builder) {
            data.src = _sceneRes;
            data.dst = _swapRes;

            builder.read(data.src, dk::ResourceUsage::ShaderRead);
            builder.write(data.dst, dk::ResourceUsage::ColorAttachment);

            builder.setColorAttachment(
                0,
                data.dst,
                LoadOp::DontCare,
                StoreOp::Store,
                std::nullopt
            );
        },
        // execute：真正录制 FXAA 绘制命令
        [this](FxaaPassData const& data, dk::RenderTaskContext& ctx) {
            this->record(ctx, data);
        }
    );
}
