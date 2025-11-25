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

    // 1. 创建 sampler（给采样源用）
    VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    samplerInfo.magFilter    = VK_FILTER_LINEAR;
    samplerInfo.minFilter    = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode   = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.maxLod       = 1000.0f;
    VK_CHECK(vkCreateSampler(device, &samplerInfo, nullptr, &_sampler));

    // 2. 描述符布局：binding0 = sampler2D uSrc, binding1 = storage image uDst
    VkDescriptorSetLayoutBinding bindings[2]{};

    vkcore::DescriptorSetLayout::Builder layout_builder(ctx);
    layout_builder
       .addBinding(0, DescriptorType::eSampledImage,
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

    // 2. 加载着色器
    std::unique_ptr<vkcore::ShaderModule> comp_module;
    try
    {
        namespace fs = std::filesystem;
        fs::path current_dir = fs::current_path();
        // **你需要创建这两个新的着色器文件**
        fs::path target_file_comp = absolute(current_dir / "../../assets/shaders/spv/fluid/spring.mesh.spv");

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
            desc.usage = ImageUsageFlagBits::eStorage | ImageUsageFlagBits::eSampled;

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

            Extent2D extent{ data.src->desc().width, data.src->desc().height };
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

            Extent2D extent{ data.src->desc().width, data.src->desc().height };
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
    VkDevice        device = _ctx->getDevice();
    //VkCommandBuffer cmd    = ctx.frame_data->command_buffer_graphic->getHandle();
    auto cmd = ctx.frame_data->command_buffer_graphic->getHandle();
    // 1. 布局转换：src 用作 sampled image，dst 用作 storage image
    vkutil::transition_image(cmd, src->getVkImage(),
                             VK_IMAGE_LAYOUT_GENERAL,           // 你当前记录的 layout
                             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkutil::transition_image(cmd, dst->getVkImage(),
                             VK_IMAGE_LAYOUT_GENERAL,
                             VK_IMAGE_LAYOUT_GENERAL); // storage image 一般用 GENERAL


    vk::DescriptorSet frame_set = ctx.frame_data->_dynamicDescriptorAllocator->allocate(*_layout);

    vkcore::DescriptorSetWriter writer;
    writer.writeImage(0, vk::DescriptorType::eCombinedImageSampler, , 0);
    writer.writeImage(1, vk::DescriptorType::eStorageImage, , 0);
    writer.update(*ctx.vkCtx, frame_set);

    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, _pipeline->getHandle());
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, _pipeline_layout->getHandle(), 0, { frame_set },
        {});

    // 4. push constants: direction + texelSize
    float pc[4];
    pc[0] = horizontal ? 1.0f : 0.0f; // direction.x
    pc[1] = horizontal ? 0.0f : 1.0f; // direction.y
    pc[2] = 1.0f / static_cast<float>(extent.width);
    pc[3] = 1.0f / static_cast<float>(extent.height);
    cmd.pushConstants(
        _pipeline_layout->getHandle(),
        vk::ShaderStageFlagBits::eMeshEXT,
        0, sizeof(PushConstantData), &pc
    );

    uint32_t groupSizeX = 16;
    uint32_t groupSizeY = 16;
    uint32_t gx = (extent.width + groupSizeX - 1) / groupSizeX;
    uint32_t gy = (extent.height + groupSizeY - 1) / groupSizeY;

    cmd.dispatch(gx, gy, 1);

    // 2. 分配/更新 descriptor set
    VkDescriptorSetAllocateInfo allocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocInfo.descriptorPool     = _descPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &_setLayout;

    VkDescriptorSet set;
    VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &set));

    // src 视图 + sampler
    VkDescriptorImageInfo srcInfo{};
    srcInfo.sampler     = _sampler;
    srcInfo.imageView   = src->view;
    srcInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // dst storage view
    VkDescriptorImageInfo dstInfo{};
    dstInfo.imageView   = dst->view;
    dstInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkWriteDescriptorSet writes[2]{};

    writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet          = set;
    writes[0].dstBinding      = 0;
    writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[0].descriptorCount = 1;
    writes[0].pImageInfo      = &srcInfo;

    writes[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet          = set;
    writes[1].dstBinding      = 1;
    writes[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[1].descriptorCount = 1;
    writes[1].pImageInfo      = &dstInfo;

    vkUpdateDescriptorSets(device, 2, writes, 0, nullptr);


    // **核心变化**: 绑定3个缓冲区到描述符集

    // 3. 绑定 pipeline + descriptor
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            _pipelineLayout, 0, 1, &set, 0, nullptr);

    // 4. push constants: direction + texelSize
    float pc[4];
    pc[0] = horizontal ? 1.0f : 0.0f; // direction.x
    pc[1] = horizontal ? 0.0f : 1.0f; // direction.y
    pc[2] = 1.0f / static_cast<float>(extent.width);
    pc[3] = 1.0f / static_cast<float>(extent.height);

    vkCmdPushConstants(cmd, _pipelineLayout,
                       VK_SHADER_STAGE_COMPUTE_BIT,
                       0, sizeof(pc), pc);

    // 5. dispatch
    uint32_t groupSizeX = 16;
    uint32_t groupSizeY = 16;
    uint32_t gx         = (extent.width + groupSizeX - 1) / groupSizeX;
    uint32_t gy         = (extent.height + groupSizeY - 1) / groupSizeY;

    vkCmdDispatch(cmd, gx, gy, 1);

    // 这里不立刻做 memory barrier，让 RG 的同步 / 后续 pass 自己决定（或者你在 RG 里抽一层）
}
} // namespace dk
