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

class DistortionPass
{
public:
    void init(vkcore::VulkanContext* ctx);

    // input: 场景颜色
    // output: 失真后的颜色
    void registerToGraph(RenderGraph&                          graph,
                         RGResource<ImageDesc, FrameGraphImage>* input,
                         RGResource<ImageDesc, FrameGraphImage>* output);

    void setStrength(float strength) { _strength = strength; }
    void setFrequency(float frequency) { _frequency = frequency; }

private:
    void recordDistortion(RenderGraphContext& ctx,
                          FrameGraphImage*    src,
                          FrameGraphImage*    dst,
                          vk::Extent2D        extent);

    vkcore::VulkanContext* _ctx = nullptr;

    std::unique_ptr<vkcore::DescriptorSetLayout> _desc_layout{nullptr};
    std::unique_ptr<vkcore::PipelineLayout>      _pipeline_layout;
    std::unique_ptr<vkcore::Pipeline>            _pipeline;

    std::unique_ptr<vkcore::SamplerResource> _sampler{nullptr};

    float _strength  = 0.035f;
    float _frequency = 24.0f;
};
} // namespace dk
