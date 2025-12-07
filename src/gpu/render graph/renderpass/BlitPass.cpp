#include "BlitPass.h"

#include "vk_images.h"
#include "vk_initializers.h"
#include "render graph/RenderGraph.h"
#include "render graph/RenderTaskBuilder.h"
#include "render graph/ResourceTexture.h"
#include "Vulkan/CommandBuffer.h"
#include "Vulkan/DescriptorSetLayout.h"
#include "Vulkan/DescriptorSetPool.h"
#include "Vulkan/DescriptorWriter.h"
#include "Vulkan/Pipeline.h"
#include "Vulkan/PipelineLayout.h"
#include "Vulkan/ShaderModule.h"

namespace dk {
void BlitPass::init(vkcore::VulkanContext& ctx)
{
}

void BlitPass::destroy(vkcore::VulkanContext& ctx)
{
}
void BlitPass::registerToGraph(RenderGraph& graph, RGResource<ImageDesc, FrameGraphImage>* sceneColor, RGResource<ImageDesc, FrameGraphImage>* swapchainColor)
{
    _sceneRes = sceneColor;
    _swapRes = swapchainColor;

    graph.addTask<BlitPassData>(
        "Blit Pass",
        // setup：声明资源读写 + attachment
        [this](BlitPassData& data, RenderTaskBuilder& builder)
        {
            data.src = _sceneRes;
            data.dst = _swapRes;

            builder.read(data.src);
            builder.write(data.dst);
        },
        // execute：真正录制 FXAA 绘制命令
        [this](const BlitPassData& data, RenderGraphContext& ctx)
        {
            this->record(ctx, data);
        }
    );
}

void BlitPass::record(RenderGraphContext& ctx, const BlitPassData& data)
{
    auto*       srcImg   = data.src->get();
    auto*       dstImg   = data.dst->get();
    const auto& src_desc = data.src->desc();
    const auto& dst_desc = data.dst->desc();
    VkImage     src      = srcImg->getVkImage();
    VkImage     dst      = dstImg->getVkImage();
    //ctx.frame_data->command_buffer_graphic->begin();
    // 1. 做布局转换（照抄你原来 copy_image_to_image 前的逻辑）
    vkutil::transition_image(ctx.frame_data->command_buffer_graphic->getHandle(), src,
                                        VK_IMAGE_LAYOUT_GENERAL, // 这里按你 draw_main 结束时的布局来填
                             VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    vkutil::transition_image(ctx.frame_data->command_buffer_graphic->getHandle(), dst,
        VK_IMAGE_LAYOUT_UNDEFINED, // 当前刚 acquire 的 swapchain image
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // 2. 填 blit 区域（全屏 blit）
    VkImageBlit blitRegion{};
    blitRegion.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    blitRegion.srcSubresource.mipLevel       = 0;
    blitRegion.srcSubresource.baseArrayLayer = 0;
    blitRegion.srcSubresource.layerCount     = 1;
    blitRegion.srcOffsets[0]                 = {0, 0, 0};
    blitRegion.srcOffsets[1]                 = {
        static_cast<int32_t>(src_desc.width),
        static_cast<int32_t>(src_desc.height),
        static_cast<int32_t>(src_desc.depth)
    };

    blitRegion.dstSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    blitRegion.dstSubresource.mipLevel       = 0;
    blitRegion.dstSubresource.baseArrayLayer = 0;
    blitRegion.dstSubresource.layerCount     = 1;
    blitRegion.dstOffsets[0]                 = {0, 0, 0};
    blitRegion.dstOffsets[1]                 = {
        static_cast<int32_t>(dst_desc.width),
        static_cast<int32_t>(dst_desc.height),
        static_cast<int32_t>(dst_desc.depth)
    };

    vkCmdBlitImage(ctx.frame_data->command_buffer_graphic->getHandle(),
                   src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   1, &blitRegion,
                   VK_FILTER_LINEAR);

    // 3. 把 swapchain image 布局准备给后续（比如 imgui / present）
    //    你之前是先转 COLOR_ATTACHMENT_OPTIMAL 画 imgui，再转 PRESENT
    vkutil::transition_image(ctx.frame_data->command_buffer_graphic->getHandle(), dst,
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_GENERAL);

    vkutil::transition_image(ctx.frame_data->command_buffer_graphic->getHandle(), src,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_IMAGE_LAYOUT_GENERAL);
    //ctx.frame_data->command_buffer_graphic->end();
}

void BlitPass::createBuffers()
{
}

void BlitPass::createDescriptors()
{
}

void BlitPass::createPipeline(vk::Format color_format, vk::Format depth_format)
{
    // 1. 创建布局
    {
        // **核心变化**: 我们现在需要3个绑定
        vkcore::DescriptorSetLayout::Builder layout_builder(_context);
        layout_builder
           .addBinding(0, vk::DescriptorType::eUniformBuffer,
                       vk::ShaderStageFlagBits::eMeshEXT) // binding=0: Camera UBO
           .addBinding(1, vk::DescriptorType::eStorageBuffer,
                       vk::ShaderStageFlagBits::eMeshEXT) // binding=1: Particle Data SSBO
           .addBinding(2, vk::DescriptorType::eStorageBuffer,
                       vk::ShaderStageFlagBits::eMeshEXT); // binding=2: Spring Index SSBO
        _layout = layout_builder.build();

        vk::PushConstantRange push_constant_range;
        push_constant_range.stageFlags = vk::ShaderStageFlagBits::eMeshEXT;
        push_constant_range.offset     = 0;
        push_constant_range.size       = sizeof(PushConstantData);

        _pipeline_layout = std::make_unique<vkcore::PipelineLayout>(
            _context,
            std::vector<vkcore::DescriptorSetLayout*>{_layout.get()},
            std::vector<vk::PushConstantRange>{push_constant_range}
        );
    }

    // 2. 加载着色器
    std::unique_ptr<vkcore::ShaderModule> mesh_module;
    std::unique_ptr<vkcore::ShaderModule> frag_module;
    try
    {
        namespace fs = std::filesystem;
        fs::path current_dir = fs::current_path();
        // **你需要创建这两个新的着色器文件**
        fs::path target_file_mesh = absolute(current_dir / "../../assets/shaders/spv/fluid/spring.mesh.spv");
        fs::path target_file_frag = absolute(current_dir / "../../assets/shaders/spv/fluid/spring.frag.spv");

        mesh_module = std::make_unique<vkcore::ShaderModule>(_context, vkcore::loadSpirvFromFile(target_file_mesh));
        frag_module = std::make_unique<vkcore::ShaderModule>(_context, vkcore::loadSpirvFromFile(target_file_frag));
    }
    catch (const std::runtime_error& e)
    {
        throw std::runtime_error("Failed to load spring shaders. Error: " + std::string(e.what()));
    }

    // 3. 创建管线
    vkcore::PipelineBuilder builder(_context);
    _pipeline = builder.setLayout(_pipeline_layout.get())
                       .setShaders(mesh_module->getHandle(), frag_module->getHandle())
                       .setRenderingInfo({color_format}, depth_format)
                        // **核心变化**: 设置为线框模式
                       .setPolygonMode(vk::PolygonMode::eLine)
                       .setCullMode(vk::CullModeFlagBits::eNone)
                       .enableDepthTest(true, vk::CompareOp::eLessOrEqual)
                       .disableColorBlending()
                       .build();
}
}
