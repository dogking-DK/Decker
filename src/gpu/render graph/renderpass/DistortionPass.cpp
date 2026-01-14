#include "DistortionPass.h"

#include "vk_engine.h"
#include "vk_images.h"
#include "vk_initializers.h"
#include "render graph/RenderGraph.h"
#include "render graph/ResourceTexture.h"
#include "Vulkan/DescriptorSetLayout.h"
#include "Vulkan/DescriptorSetPool.h"
#include "Vulkan/DescriptorWriter.h"
#include "Vulkan/Pipeline.h"
#include "Vulkan/PipelineLayout.h"
#include "Vulkan/Sampler.h"
#include "Vulkan/ShaderModule.h"

using namespace vk;

namespace dk {
struct DistortionPassData
{
    RGResource<ImageDesc, FrameGraphImage>* src = nullptr;
    RGResource<ImageDesc, FrameGraphImage>* dst = nullptr;
};

void DistortionPass::init(vkcore::VulkanContext* ctx)
{
    _ctx            = ctx;

    _sampler = std::make_unique<vkcore::SamplerResource>(*ctx, vkcore::makeLinearClampSamplerInfo());

    vkcore::DescriptorSetLayout::Builder layout_builder(ctx);
    layout_builder
       .addBinding(0, DescriptorType::eCombinedImageSampler,
                   ShaderStageFlagBits::eCompute)
       .addBinding(1, DescriptorType::eStorageImage,
                   ShaderStageFlagBits::eCompute);
    _desc_layout = layout_builder.build();

    PushConstantRange push_constant_range{};
    push_constant_range.stageFlags = ShaderStageFlagBits::eCompute;
    push_constant_range.offset     = 0;
    push_constant_range.size       = sizeof(float) * 4;

    _pipeline_layout = std::make_unique<vkcore::PipelineLayout>(
        ctx,
        std::vector<vkcore::DescriptorSetLayout*>{_desc_layout.get()},
        std::vector<PushConstantRange>{push_constant_range}
    );
    _pipeline_layout->setDebugName("distortion");

    std::unique_ptr<vkcore::ShaderModule> comp_module;
    try
    {
        namespace fs = std::filesystem;
        fs::path current_dir = fs::current_path();
        fs::path target_file_comp = absolute(current_dir / "../../assets/shaders/spv/common/distortion.comp.spv");

        comp_module = std::make_unique<vkcore::ShaderModule>(ctx, vkcore::loadSpirvFromFile(target_file_comp));
    }
    catch (const std::runtime_error& e)
    {
        throw std::runtime_error("Failed to load distortion shader. Error: " + std::string(e.what()));
    }

    vkcore::PipelineBuilder builder(ctx);
    _pipeline = builder.setLayout(_pipeline_layout.get())
                       .setComputeShader(comp_module->getHandle())
                       .build();
}

void DistortionPass::registerToGraph(RenderGraph&                          graph,
                                     RGResource<ImageDesc, FrameGraphImage>* input,
                                     RGResource<ImageDesc, FrameGraphImage>* output)
{
    graph.addTask<DistortionPassData>(
        "Distortion",
        [&](DistortionPassData& data, RenderTaskBuilder& builder)
        {
            data.src = input;
            data.dst = output;

            builder.read(input);
            builder.write(output);
        },
        [this](const DistortionPassData& data, RenderGraphContext& ctx)
        {
            auto* srcImg = data.src->get();
            auto* dstImg = data.dst->get();

            Extent2D extent{data.src->desc().width, data.src->desc().height};
            recordDistortion(ctx, srcImg, dstImg, extent);
        }
    );
}

void DistortionPass::recordDistortion(RenderGraphContext& ctx,
                                      FrameGraphImage*    src,
                                      FrameGraphImage*    dst,
                                      Extent2D            extent)
{
    auto cmd = ctx.frame_data->command_buffer_graphic->getHandle();

    vkutil::transition_image(cmd, src->getVkImage(),
                             VK_IMAGE_LAYOUT_GENERAL,
                             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkutil::transition_image(cmd, dst->getVkImage(),
                             VK_IMAGE_LAYOUT_UNDEFINED,
                             VK_IMAGE_LAYOUT_GENERAL);

    DescriptorSet frame_set = ctx.frame_data->_dynamicDescriptorAllocator->allocate(*_desc_layout);

    vkcore::DescriptorSetWriter writer;
    auto combined_image_info = vkcore::makeCombinedImageInfo(src->getVkImageView(), _sampler->getHandle(),
                                                             ImageLayout::eShaderReadOnlyOptimal);
    auto storage_image_info = vkcore::makeStorageImageInfo(dst->getVkImageView(), ImageLayout::eGeneral);
    writer.writeImage(0, DescriptorType::eCombinedImageSampler, combined_image_info, 0);
    writer.writeImage(1, DescriptorType::eStorageImage, storage_image_info, 0);
    writer.update(*ctx.vkCtx, frame_set);

    cmd.bindPipeline(PipelineBindPoint::eCompute, _pipeline->getHandle());
    cmd.bindDescriptorSets(PipelineBindPoint::eCompute, _pipeline_layout->getHandle(), 0, {frame_set}, {});

    float pc[4];
    pc[0] = _strength;
    pc[1] = _frequency;
    pc[2] = 0.0f; // time or phase, reserved for animation
    pc[3] = static_cast<float>(extent.width) / static_cast<float>(extent.height);

    cmd.pushConstants(
        _pipeline_layout->getHandle(),
        ShaderStageFlagBits::eCompute,
        0, sizeof(float) * 4, &pc
    );

    const uint32_t groupSizeX = 16;
    const uint32_t groupSizeY = 16;
    const uint32_t gx         = (extent.width + groupSizeX - 1) / groupSizeX;
    const uint32_t gy         = (extent.height + groupSizeY - 1) / groupSizeY;

    cmd.dispatch(gx, gy, 1);
}
} // namespace dk
