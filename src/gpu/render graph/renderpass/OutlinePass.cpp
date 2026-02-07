#include "OutlinePass.h"

#include <filesystem>
#include <stdexcept>

#include "Vulkan/CommandBuffer.h"
#include "Vulkan/ShaderModule.h"
#include "Vulkan/ShaderCompiler.h"

namespace dk::render {
namespace {
std::string build_shader_path(const std::filesystem::path& filename)
{
    namespace fs = std::filesystem;
    fs::path current_dir = get_exe_dir();
    return absolute(current_dir / "assets/shaders/spv/common" / filename).string();
}
} // namespace

OutlinePass::OutlinePass(vkcore::VulkanContext& ctx)
    : _context(ctx)
{
}

void OutlinePass::init(vk::Format color_format, vk::Format depth_format)
{
    vkcore::ShaderCompiler::initGlslang();

    std::unique_ptr<vkcore::ShaderModule> vert_module;
    std::unique_ptr<vkcore::ShaderModule> frag_module;
    try
    {
        vert_module = std::make_unique<vkcore::ShaderModule>(
            &_context, vkcore::loadSpirvFromFile(build_shader_path("outline_extrude.vert.spv")));
        frag_module = std::make_unique<vkcore::ShaderModule>(
            &_context, vkcore::loadSpirvFromFile(build_shader_path("outline.frag.spv")));
    }
    catch (const std::runtime_error& e)
    {
        throw std::runtime_error("Failed to load outline shaders. Error: " + std::string(e.what()));
    }

    vk::PushConstantRange push_range;
    push_range.stageFlags = vk::ShaderStageFlagBits::eVertex;
    push_range.offset     = 0;
    push_range.size       = sizeof(PushConstants);

    _pipeline_layout = std::make_unique<vkcore::PipelineLayout>(
        &_context,
        std::vector<vkcore::DescriptorSetLayout*>{},
        std::vector<vk::PushConstantRange>{push_range});

    std::vector<vk::VertexInputBindingDescription> bindings = {
        {
            0,
            sizeof(GPUVertex),
            vk::VertexInputRate::eVertex
        }
    };

    std::vector<vk::VertexInputAttributeDescription> attributes = {
        {
            0,
            0,
            vk::Format::eR32G32B32Sfloat,
            offsetof(GPUVertex, position)
        },
        {
            1,
            0,
            vk::Format::eR32G32B32Sfloat,
            offsetof(GPUVertex, normal)
        },
        {
            2,
            0,
            vk::Format::eR32G32B32A32Sfloat,
            offsetof(GPUVertex, tangent)
        },
        {
            3,
            0,
            vk::Format::eR32G32Sfloat,
            offsetof(GPUVertex, texcoord0)
        },
        {
            4,
            0,
            vk::Format::eR32G32B32A32Sfloat,
            offsetof(GPUVertex, color0)
        }
    };

    vkcore::PipelineBuilder builder(&_context);
    _pipeline = builder.setLayout(_pipeline_layout.get())
                       .setGraphicsShaders(vert_module->getHandle(), frag_module->getHandle())
                       .setVertexInput(bindings, attributes)
                       .setRenderingInfo({color_format}, depth_format)
                       .setPrimitiveTopology(vk::PrimitiveTopology::eTriangleList)
                       .setPolygonMode(vk::PolygonMode::eFill)
                       .setCullMode(vk::CullModeFlagBits::eFront)
                       .enableDepthTest(false, vk::CompareOp::eLessOrEqual)
                       .setColorBlendingAlpha()
                       .build();

    vkcore::ShaderCompiler::finalizeGlslang();
}

void OutlinePass::registerToGraph(RenderGraph& graph,
                                  RGResource<ImageDesc, FrameGraphImage>* color,
                                  RGResource<ImageDesc, FrameGraphImage>* depth)
{
    graph.addTask<OutlinePassData>(
        "Outline Pass",
        [color, depth](OutlinePassData& data, RenderTaskBuilder& builder)
        {
            data.color = color;
            data.depth = depth;
            if (data.color)
            {
                builder.write(data.color);
            }
            if (data.depth)
            {
                builder.read(data.depth);
            }
        },
        [this](const OutlinePassData& data, RenderGraphContext& ctx)
        {
            record(ctx, data);
        });
}

void OutlinePass::record(RenderGraphContext& ctx, const OutlinePassData& data) const
{
    if (!_frame_ctx || !_draw_lists || !_pipeline)
    {
        return;
    }

    if (_draw_lists->outline.empty())
    {
        return;
    }

    if (!data.color || !data.depth)
    {
        return;
    }

    auto* color_image = data.color->get();
    auto* depth_image = data.depth->get();
    if (!color_image || !depth_image)
    {
        return;
    }

    const auto& color_desc = data.color->desc();
    const vk::Rect2D render_area{
        {0, 0},
        {color_desc.width, color_desc.height}
    };

    vk::RenderingAttachmentInfo color_attachment{};
    color_attachment.imageView   = color_image->getVkImageView();
    color_attachment.imageLayout = vk::ImageLayout::eGeneral;
    color_attachment.loadOp      = vk::AttachmentLoadOp::eLoad;
    color_attachment.storeOp     = vk::AttachmentStoreOp::eStore;

    vk::RenderingAttachmentInfo depth_attachment{};
    depth_attachment.imageView   = depth_image->getVkImageView();
    depth_attachment.imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
    depth_attachment.loadOp      = vk::AttachmentLoadOp::eLoad;
    depth_attachment.storeOp     = vk::AttachmentStoreOp::eStore;

    auto& command_buffer = *ctx.frame_data->command_buffer_graphic;
    command_buffer.beginRenderingColor(render_area, color_attachment, &depth_attachment);

    auto cmd = ctx.frame_data->command_buffer_graphic->getHandle();

    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, _pipeline->getHandle());

    const vk::Viewport viewport{
        0.0f,
        static_cast<float>(_frame_ctx->viewport.height),
        static_cast<float>(_frame_ctx->viewport.width),
        -static_cast<float>(_frame_ctx->viewport.height),
        0.0f,
        1.0f
    };
    cmd.setViewport(0, 1, &viewport);

    const vk::Rect2D scissor{
        {0, 0},
        {_frame_ctx->viewport.width, _frame_ctx->viewport.height}
    };
    cmd.setScissor(0, 1, &scissor);

    vk::Buffer last_vertex_buffer{};
    vk::Buffer last_index_buffer{};

    for (const auto& item : _draw_lists->outline)
    {
        if (!item.mesh || !item.mesh->vertex_buffer || !item.mesh->index_buffer)
        {
            continue;
        }

        const auto& vb = item.mesh->vertex_buffer->getHandle();
        const auto& ib = item.mesh->index_buffer->getHandle();

        if (vb != last_vertex_buffer)
        {
            constexpr vk::DeviceSize offset = 0;
            cmd.bindVertexBuffers(0, 1, &vb, &offset);
            last_vertex_buffer = vb;
        }

        if (ib != last_index_buffer)
        {
            cmd.bindIndexBuffer(ib, 0, vk::IndexType::eUint32);
            last_index_buffer = ib;
        }

        PushConstants push{};
        push.viewproj = _frame_ctx->viewproj;
        push.model    = item.world;
        push.params   = glm::vec4(0.02f, 0.0f, 0.0f, 0.0f);
        cmd.pushConstants(_pipeline_layout->getHandle(), vk::ShaderStageFlagBits::eVertex, 0,
                          sizeof(PushConstants), &push);

        cmd.drawIndexed(item.mesh->index_count, 1, 0, 0, 0);
    }

    command_buffer.endRendering();
}
} // namespace dk::render
