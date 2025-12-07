#pragma once
#include "render graph/Resource.h"
#include "Vulkan/Sampler.h"

namespace dk {
class RenderGraph;
class FrameGraphImage;
struct ImageDesc;

namespace vkcore {
    class PipelineLayout;
    class Pipeline;
    class DescriptorSetLayout;
    class VulkanContext;
}

class GaussianBlurPass
{
public:
    void init(vkcore::VulkanContext* ctx);

    // input: 上一阶段的场景颜色（比如 blit 过来的）
    // output: 模糊结果
    void registerToGraph(RenderGraph&                          graph,
                         RGResource<ImageDesc, FrameGraphImage>* input,
                         RGResource<ImageDesc, FrameGraphImage>* output);

private:
    void recordBlur(RenderGraphContext& ctx,
                    FrameGraphImage*    src,
                    FrameGraphImage*    dst,
                    vk::Extent2D        extent,
                    bool                horizontal);

    vkcore::VulkanContext* _ctx = nullptr;

    std::unique_ptr<vkcore::DescriptorSetLayout> _desc_layout{nullptr};
    // 管线相关
    std::unique_ptr<vkcore::PipelineLayout> _pipeline_layout;
    std::unique_ptr<vkcore::Pipeline>       _pipeline;

    std::unique_ptr<vkcore::SamplerResource> _sampler{nullptr};
};
} // namespace dk
