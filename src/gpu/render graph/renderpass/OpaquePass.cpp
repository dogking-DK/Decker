#include "OpaquePass.h"

#include <filesystem>
#include <fstream>
#include <stdexcept>

#include "Vulkan/ShaderModule.h"
#include "Vulkan/ShaderCompiler.h"

namespace dk::render {
namespace {
std::string read_text(const std::filesystem::path& path)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open shader file: " + path.string());
    }
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}
} // namespace

OpaquePass::OpaquePass(vkcore::VulkanContext& ctx)
    : _context(ctx)
{
}

void OpaquePass::init(vk::Format color_format, vk::Format depth_format)
{
    vkcore::ShaderCompiler::initGlslang();

    namespace fs = std::filesystem;
    const fs::path base_dir   = fs::current_path();
    const fs::path vert_path  = base_dir / "../../assets/shaders/basic_mesh.vert";
    const fs::path frag_path  = base_dir / "../../assets/shaders/basic_mesh.frag";

    std::vector<uint32_t> vert_spv;
    std::vector<uint32_t> frag_spv;
    vkcore::ShaderCompiler compiler;
    if (!compiler.compileGLSLToSPV(read_text(vert_path), vert_spv, vk::ShaderStageFlagBits::eVertex))
    {
        throw std::runtime_error("Failed to compile basic_mesh.vert");
    }
    if (!compiler.compileGLSLToSPV(read_text(frag_path), frag_spv, vk::ShaderStageFlagBits::eFragment))
    {
        throw std::runtime_error("Failed to compile basic_mesh.frag");
    }

    auto vert_module = std::make_unique<vkcore::ShaderModule>(&_context, vert_spv);
    auto frag_module = std::make_unique<vkcore::ShaderModule>(&_context, frag_spv);

    vk::PushConstantRange push_range;
    push_range.stageFlags = vk::ShaderStageFlagBits::eVertex;
    push_range.offset     = 0;
    push_range.size       = sizeof(PushConstants);

    _pipeline_layout = std::make_unique<vkcore::PipelineLayout>(
        &_context,
        std::vector<vkcore::DescriptorSetLayout*>{},
        std::vector<vk::PushConstantRange>{push_range});

    std::vector<vk::VertexInputBindingDescription> bindings = {
        vk::VertexInputBindingDescription{
            0,
            sizeof(GPUVertex),
            vk::VertexInputRate::eVertex
        }
    };

    std::vector<vk::VertexInputAttributeDescription> attributes = {
        vk::VertexInputAttributeDescription{
            0,
            0,
            vk::Format::eR32G32B32Sfloat,
            offsetof(GPUVertex, position)
        },
        vk::VertexInputAttributeDescription{
            1,
            0,
            vk::Format::eR32G32B32A32Sfloat,
            offsetof(GPUVertex, color0)
        }
    };

    vkcore::PipelineBuilder builder(&_context);
    _pipeline = builder.setLayout(_pipeline_layout.get())
                       .setShaders(vert_module->getHandle(), frag_module->getHandle())
                       .setVertexInput(bindings, attributes)
                       .setRenderingInfo({color_format}, depth_format)
                       .setCullMode(vk::CullModeFlagBits::eBack)
                       .enableDepthTest(true, vk::CompareOp::eLessOrEqual)
                       .disableColorBlending()
                       .build();

    vkcore::ShaderCompiler::finalizeGlslang();
}

void OpaquePass::registerToGraph(RenderGraph& graph)
{
    graph.addTask<OpaquePassData>(
        "Opaque Pass",
        [](OpaquePassData&, RenderTaskBuilder&) {},
        [this](const OpaquePassData&, ::dk::RenderGraphContext& ctx)
        {
            record(ctx);
        }
    );
}

void OpaquePass::record(::dk::RenderGraphContext& ctx) const
{
    if (!_frame_ctx || !_draw_lists || !_pipeline)
    {
        return;
    }

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

    for (const auto& item : _draw_lists->opaque)
    {
        if (!item.mesh || !item.mesh->vertex_buffer || !item.mesh->index_buffer)
        {
            continue;
        }

        const auto& vb = item.mesh->vertex_buffer->getHandle();
        const auto& ib = item.mesh->index_buffer->getHandle();

        if (vb != last_vertex_buffer)
        {
            const vk::DeviceSize offset = 0;
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
        cmd.pushConstants(_pipeline_layout->getHandle(), vk::ShaderStageFlagBits::eVertex, 0,
                          sizeof(PushConstants), &push);

        cmd.drawIndexed(item.mesh->index_count, 1, 0, 0, 0);
    }
}
} // namespace dk::render
