#pragma once
#include "render graph/Resource.h"

namespace dk {
class FrameGraphImage;
struct ImageDesc;
class RenderGraph;

namespace vkcore {
    class Pipeline;
    class PipelineLayout;
    class DescriptorSetLayout;
    class BufferResource;
    class VulkanContext;
}

class BlitPass
{
public:
    struct BlitPassData
    {
        RGResource<ImageDesc, FrameGraphImage>* src;
        RGResource<ImageDesc, FrameGraphImage>* dst;
    };

    void init(vkcore::VulkanContext& ctx);  // 创建 shader module / pipeline / layout / sampler 等
    void destroy(vkcore::VulkanContext& ctx);

    // 把 FXAA 作为一个 Task 注册进 RenderGraph
    void registerToGraph(
        RenderGraph& graph,
        RGResource<ImageDesc, FrameGraphImage>* sceneColor,
        RGResource<ImageDesc, FrameGraphImage>* swapchainColor
    );
    void setSrc(RGResource<ImageDesc, FrameGraphImage>* src) { _sceneRes = src; }
private:
    void record(dk::RenderGraphContext& ctx, BlitPassData const& data);
    void createBuffers();
    void createDescriptors();
    void createPipeline(vk::Format color_format, vk::Format depth_format);

    vkcore::VulkanContext* _context{nullptr};


    RGResource<ImageDesc, FrameGraphImage>* _sceneRes{nullptr};
    RGResource<ImageDesc, FrameGraphImage>* _swapRes{nullptr};

    // GPU 资源
    std::unique_ptr<vkcore::BufferResource> _particle_data_ssbo; // 存储所有粒子的位置和颜色
    std::unique_ptr<vkcore::BufferResource> _spring_index_ssbo;  // 存储弹簧的索引对
    std::unique_ptr<vkcore::BufferResource> _camera_ubo;

    // 描述符相关
    std::unique_ptr<vkcore::DescriptorSetLayout>         _layout;
    std::unique_ptr<vkcore::GrowableDescriptorAllocator> _frame_allocator;

    // 管线相关
    std::unique_ptr<vkcore::PipelineLayout> _pipeline_layout;
    std::unique_ptr<vkcore::Pipeline>       _pipeline;
};
} // namespace dk::rg
