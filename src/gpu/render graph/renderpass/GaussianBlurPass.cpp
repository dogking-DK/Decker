// GaussianBlurPass.cpp
#include "GaussianBlurPass.h"
#include "vk_engine.h"            // 里面有 device 等
#include "vk_images.h"
#include "vk_initializers.h"      // 你自己的 helper，如果有
#include "render graph/RenderGraph.h"
#include "render graph/ResourceTexture.h"
#include "Vulkan/DescriptorSetLayout.h"
#include "Vulkan/DescriptorSetPool.h"
#include "Vulkan/DescriptorWriter.h"
#include "Vulkan/Pipeline.h"
#include "Vulkan/PipelineLayout.h"
#include "Vulkan/Sampler.h"
#include "Vulkan/ShaderModule.h"

using namespace vk; // 如果你在用 Vulkan-Hpp

namespace dk {
struct BlurPassData
{
    Resource<ImageDesc, FrameGraphImage>* src = nullptr;
    Resource<ImageDesc, FrameGraphImage>* dst = nullptr;
};

void GaussianBlurPass::init(vkcore::VulkanContext* ctx)
{
    _ctx            = ctx;
    VkDevice device = _ctx->getDevice();

    _sampler = std::make_unique<vkcore::SamplerResource>(*ctx, vkcore::makeLinearClampSamplerInfo());

    // 2. 描述符布局：binding0 = sampler2D uSrc, binding1 = storage image uDst
    VkDescriptorSetLayoutBinding bindings[2]{};

    vkcore::DescriptorSetLayout::Builder layout_builder(ctx);
    layout_builder
       .addBinding(0, DescriptorType::eCombinedImageSampler,
                   ShaderStageFlagBits::eCompute) // binding=0: Camera UBO
       .addBinding(1, DescriptorType::eStorageImage,
                   ShaderStageFlagBits::eCompute); // binding=1: Particle Data SSBO
    _desc_layout = layout_builder.build();


    PushConstantRange push_constant_range;
    push_constant_range.stageFlags = ShaderStageFlagBits::eCompute;
    push_constant_range.offset     = 0;
    push_constant_range.size       = sizeof(float) * 4;

    _pipeline_layout = std::make_unique<vkcore::PipelineLayout>(
        ctx,
        std::vector<vkcore::DescriptorSetLayout*>{_desc_layout.get()},
        std::vector<PushConstantRange>{push_constant_range}
    );
    _pipeline_layout->setDebugName("gaussian blur");
    // 2. 加载着色器
    std::unique_ptr<vkcore::ShaderModule> comp_module;
    try
    {
        namespace fs = std::filesystem;
        fs::path current_dir = fs::current_path();
        // **你需要创建这两个新的着色器文件**
        fs::path target_file_comp = absolute(current_dir / "../../assets/shaders/spv/common/gaussian_blur.comp.spv");

        comp_module = std::make_unique<vkcore::ShaderModule>(ctx, vkcore::loadSpirvFromFile(target_file_comp));
    }
    catch (const std::runtime_error& e)
    {
        throw std::runtime_error("Failed to load spring shaders. Error: " + std::string(e.what()));
    }

    // 3. 创建管线
    vkcore::PipelineBuilder builder(ctx);
    _pipeline = builder.setLayout(_pipeline_layout.get())
                       .setShaders(comp_module->getHandle())
                       .build();
}


void GaussianBlurPass::registerToGraph(RenderGraph&                          graph,
                                       Resource<ImageDesc, FrameGraphImage>* input,
                                       Resource<ImageDesc, FrameGraphImage>* output)
{
    using Res = Resource<ImageDesc, FrameGraphImage>;

    // 1. 创建一个临时的 ping-pong image（Transient）
    Res* tempRes = nullptr;
    graph.addTask<BlurPassData>(
        "GaussianBlurH_CreateTemp",
        // setup
        [&](BlurPassData& data, RenderTaskBuilder& builder)
        {
            ImageDesc desc = input->desc();
            desc.usage     = ImageUsageFlagBits::eStorage | ImageUsageFlagBits::eSampled;

            tempRes = builder.create<Res>("blurTemp", desc, ResourceLifetime::Transient);

            data.src = input;
            data.dst = tempRes;

            builder.read(input);
            builder.write(tempRes);
        },
        // execute 空的，这个 task 只是为了声明 temp 资源和依赖
        [](const BlurPassData&, RenderGraphContext&)
        {
        }
    );

    // 2. 横向 blur
    graph.addTask<BlurPassData>(
        "GaussianBlurH",
        [&](BlurPassData& data, RenderTaskBuilder& builder)
        {
            data.src = input;
            data.dst = tempRes;

            builder.read(input);
            builder.write(tempRes);
        },
        [this](const BlurPassData& data, RenderGraphContext& ctx)
        {
            auto* srcImg = data.src->get();
            auto* dstImg = data.dst->get();

            Extent2D extent{data.src->desc().width, data.src->desc().height};
            recordBlur(ctx, srcImg, dstImg, extent, /*horizontal*/ true);
        }
    );

    // 3. 纵向 blur，输入= temp，输出= output
    graph.addTask<BlurPassData>(
        "GaussianBlurV",
        [&](BlurPassData& data, RenderTaskBuilder& builder)
        {
            data.src = tempRes;
            data.dst = output;

            builder.read(tempRes);
            builder.write(output);
        },
        [this](const BlurPassData& data, RenderGraphContext& ctx)
        {
            auto* srcImg = data.src->get();
            auto* dstImg = data.dst->get();

            Extent2D extent{data.src->desc().width, data.src->desc().height};
            recordBlur(ctx, srcImg, dstImg, extent, /*horizontal*/ false);
        }
    );
}

void GaussianBlurPass::recordBlur(RenderGraphContext& ctx,
                                  FrameGraphImage*    src,
                                  FrameGraphImage*    dst,
                                  Extent2D            extent,
                                  bool                horizontal)
{
    VkDevice device = _ctx->getDevice();
    //VkCommandBuffer cmd    = ctx.frame_data->command_buffer_graphic->getHandle();
    auto cmd = ctx.frame_data->command_buffer_graphic->getHandle();
    // 1. 布局转换：src 用作 sampled image，dst 用作 storage image
    vkutil::transition_image(cmd, src->getVkImage(),
                             VK_IMAGE_LAYOUT_GENERAL,           // 你当前记录的 layout
                             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkutil::transition_image(cmd, dst->getVkImage(),
                             VK_IMAGE_LAYOUT_UNDEFINED,
                             VK_IMAGE_LAYOUT_GENERAL); // storage image 一般用 GENERAL


    DescriptorSet frame_set = ctx.frame_data->_dynamicDescriptorAllocator->allocate(*_desc_layout);

    vkcore::DescriptorSetWriter writer;
    auto combined_image_info = vkcore::makeCombinedImageInfo(src->getVkImageView(), _sampler->getHandle(),
                                                             ImageLayout::eShaderReadOnlyOptimal);
    auto storage_image_info = vkcore::makeStorageImageInfo(dst->getVkImageView(), ImageLayout::eGeneral);
    writer.writeImage(0, DescriptorType::eCombinedImageSampler, combined_image_info, 0);
    writer.writeImage(1, DescriptorType::eStorageImage, storage_image_info, 0);
    writer.update(*ctx.vkCtx, frame_set);

    cmd.bindPipeline(PipelineBindPoint::eCompute, _pipeline->getHandle());
    cmd.bindDescriptorSets(PipelineBindPoint::eCompute, _pipeline_layout->getHandle(), 0, {frame_set},
                           {});

    // 4. push constants: direction + texelSize
    float pc[4];
    pc[0] = horizontal ? 1.0f : 0.0f; // direction.x
    pc[1] = horizontal ? 0.0f : 1.0f; // direction.y
    pc[2] = 1.0f / static_cast<float>(extent.width);
    pc[3] = 1.0f / static_cast<float>(extent.height);
    cmd.pushConstants(
        _pipeline_layout->getHandle(),
        ShaderStageFlagBits::eCompute,
        0, sizeof(float) * 4, &pc
    );

    uint32_t groupSizeX = 16;
    uint32_t groupSizeY = 16;
    uint32_t gx         = (extent.width + groupSizeX - 1) / groupSizeX;
    uint32_t gy         = (extent.height + groupSizeY - 1) / groupSizeY;

    cmd.dispatch(gx, gy, 1);
}
} // namespace dk
