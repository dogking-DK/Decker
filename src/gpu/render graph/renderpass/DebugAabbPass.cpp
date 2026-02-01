#include "DebugAabbPass.h"

#include <filesystem>
#include <stdexcept>

#include "Vulkan/CommandBuffer.h"
#include "Vulkan/DescriptorSetLayout.h"
#include "Vulkan/DescriptorSetPool.h"
#include "Vulkan/DescriptorWriter.h"
#include "Vulkan/ShaderModule.h"
#include "Vulkan/ShaderCompiler.h"

namespace dk::render {
DebugAabbPass::DebugAabbPass(vkcore::VulkanContext& ctx)
    : _context(ctx)
{
}

void DebugAabbPass::init(vk::Format color_format, vk::Format depth_format)
{
    vkcore::ShaderCompiler::initGlslang();

    std::unique_ptr<vkcore::ShaderModule> mesh_module;
    std::unique_ptr<vkcore::ShaderModule> frag_module;
    try
    {
        namespace fs = std::filesystem;
        fs::path current_dir = get_exe_dir();
        fs::path target_file_mesh = absolute(current_dir / "assets/shaders/spv/common/debug_aabb.mesh.spv");
        fs::path target_file_frag = absolute(current_dir / "assets/shaders/spv/common/debug_aabb.frag.spv");

        mesh_module = std::make_unique<vkcore::ShaderModule>(&_context, vkcore::loadSpirvFromFile(target_file_mesh));
        frag_module = std::make_unique<vkcore::ShaderModule>(&_context, vkcore::loadSpirvFromFile(target_file_frag));
    }
    catch (const std::runtime_error& e)
    {
        throw std::runtime_error("Failed to load debug AABB shaders. Error: " + std::string(e.what()));
    }

    vk::PushConstantRange push_range;
    push_range.stageFlags = vk::ShaderStageFlagBits::eMeshEXT | vk::ShaderStageFlagBits::eFragment;
    push_range.offset     = 0;
    push_range.size       = sizeof(PushConstants);

    vkcore::DescriptorSetLayout::Builder layout_builder(&_context);
    layout_builder.addBinding(0, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eMeshEXT);
    _descriptor_set_layout = layout_builder.build();

    _pipeline_layout = std::make_unique<vkcore::PipelineLayout>(
        &_context,
        std::vector<vkcore::DescriptorSetLayout*>{_descriptor_set_layout.get()},
        std::vector<vk::PushConstantRange>{push_range});

    vkcore::PipelineBuilder builder(&_context);
    _pipeline = builder.setLayout(_pipeline_layout.get())
                       .setMeshShaders(mesh_module->getHandle(), frag_module->getHandle())
                       .setRenderingInfo({color_format}, depth_format)
                       .setCullMode(vk::CullModeFlagBits::eNone)
                       .enableDepthTest(false, vk::CompareOp::eLessOrEqual)
                       .setColorBlendingAlpha()
                       .build();

    vkcore::ShaderCompiler::finalizeGlslang();
}

void DebugAabbPass::registerToGraph(RenderGraph& graph)
{
    graph.addTask<DebugAabbPassData>(
        "Debug AABB Pass",
        [](DebugAabbPassData&, RenderTaskBuilder&)
        {
        },
        [this](const DebugAabbPassData&, RenderGraphContext& ctx)
        {
            record(ctx);
        }
    );
}

void DebugAabbPass::record(RenderGraphContext& ctx) const
{
    if (!_frame_ctx || !_debug_service || !_pipeline)
    {
        return;
    }

    if (!_debug_service->isEnabled())
    {
        return;
    }

    const auto* line_buffer = _debug_service->lineBuffer();
    const uint32_t line_count = _debug_service->lineCount();
    if (!line_buffer || line_count == 0)
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

    vk::DescriptorSet frame_set = ctx.frame_data->_dynamicDescriptorAllocator->allocate(*_descriptor_set_layout);
    vkcore::DescriptorSetWriter writer;
    writer.writeBuffer(0, vk::DescriptorType::eStorageBuffer, line_buffer->getDescriptorInfo());
    writer.update(*ctx.vkCtx, frame_set);
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, _pipeline_layout->getHandle(), 0, {frame_set}, {});

    PushConstants push{};
    push.viewproj = _frame_ctx->viewproj;
    push.color = glm::vec4(0.2f, 0.9f, 0.3f, 1.0f);
    cmd.pushConstants(_pipeline_layout->getHandle(), vk::ShaderStageFlagBits::eMeshEXT | vk::ShaderStageFlagBits::eFragment,
                      0, sizeof(PushConstants), &push);

    constexpr uint32_t workgroup_size = 64;
    uint32_t group_count = (line_count + workgroup_size - 1) / workgroup_size;
    cmd.drawMeshTasksEXT(group_count, 1, 1);
}
} // namespace dk::render
