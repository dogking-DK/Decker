#include "OpaquePass.h"

#include <filesystem>
#include <fstream>
#include <stdexcept>

#include "Vulkan/CommandBuffer.h"
#include "Vulkan/DescriptorSetLayout.h"
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
    const fs::path base_dir = get_exe_dir();

    std::unique_ptr<vkcore::ShaderModule> vert_module;
    std::unique_ptr<vkcore::ShaderModule> frag_module;
    try
    {
        namespace fs = std::filesystem;
        fs::path current_dir = get_exe_dir();
        // **你需要创建这两个新的着色器文件**
        fs::path target_file_mesh = absolute(current_dir / "assets/shaders/spv/common/basic_mesh.vert.spv");
        fs::path target_file_frag = absolute(current_dir / "assets/shaders/spv/common/basic_mesh.frag.spv");

        vert_module = std::make_unique<vkcore::ShaderModule>(&_context, vkcore::loadSpirvFromFile(target_file_mesh));
        frag_module = std::make_unique<vkcore::ShaderModule>(&_context, vkcore::loadSpirvFromFile(target_file_frag));
    }
    catch (const std::runtime_error& e)
    {
        throw std::runtime_error("Failed to load spring shaders. Error: " + std::string(e.what()));
    }

    vk::PushConstantRange push_range;
    push_range.stageFlags = vk::ShaderStageFlagBits::eVertex;
    push_range.offset     = 0;
    push_range.size       = sizeof(PushConstants);

    vkcore::DescriptorSetLayout::Builder layout_builder(&_context);
    layout_builder.addBinding(0, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment)
                  .addBinding(1, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment)
                  .addBinding(2, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment)
                  .addBinding(3, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment)
                  .addBinding(4, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment);
    _material_set_layout = layout_builder.build();

    _pipeline_layout = std::make_unique<vkcore::PipelineLayout>(
        &_context,
        std::vector<vkcore::DescriptorSetLayout*>{_material_set_layout.get()},
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
        [](OpaquePassData&, RenderTaskBuilder&)
        {
        },
        [this](const OpaquePassData&, RenderGraphContext& ctx)
        {
            record(ctx);
        }
    );
}

void OpaquePass::record(RenderGraphContext& ctx) const
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

    vk::Buffer        last_vertex_buffer{};
    vk::Buffer        last_index_buffer{};
    vk::DescriptorSet last_material_set{};

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
            constexpr vk::DeviceSize offset = 0;
            cmd.bindVertexBuffers(0, 1, &vb, &offset);
            last_vertex_buffer = vb;
        }

        if (ib != last_index_buffer)
        {
            cmd.bindIndexBuffer(ib, 0, vk::IndexType::eUint32);
            last_index_buffer = ib;
        }

        if (item.material && item.material->descriptor_set && item.material->descriptor_set != last_material_set)
        {
            cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, _pipeline_layout->getHandle(), 0,
                                   {item.material->descriptor_set}, {});
            last_material_set = item.material->descriptor_set;
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
