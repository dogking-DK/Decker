#include "Pipeline.h"
#include <stdexcept>

namespace dk::vkcore {
Pipeline::Pipeline(VulkanContext* context, vk::Pipeline pipeline, PipelineLayout* layout)
    : Resource(context, pipeline), _layout(layout)
{
}

Pipeline::~Pipeline()
{
    if (hasDevice() && hasHandle())
    {
        _context->getDevice().destroyPipeline(_handle);
    }
}


PipelineBuilder::PipelineBuilder(VulkanContext* context)
    : _context(context)
{
    // --- 设置所有状态的合理默认值 ---
    _input_assembly_info.topology               = vk::PrimitiveTopology::eTriangleList;
    _input_assembly_info.primitiveRestartEnable = VK_FALSE;

    _rasterization_info.depthClampEnable        = VK_FALSE;
    _rasterization_info.rasterizerDiscardEnable = VK_FALSE;
    _rasterization_info.polygonMode             = vk::PolygonMode::eFill;
    _rasterization_info.lineWidth               = 1.0f;
    _rasterization_info.cullMode                = vk::CullModeFlagBits::eBack;
    _rasterization_info.frontFace               = vk::FrontFace::eCounterClockwise;
    _rasterization_info.depthBiasEnable         = VK_FALSE;

    _multisample_info.sampleShadingEnable  = VK_FALSE;
    _multisample_info.rasterizationSamples = vk::SampleCountFlagBits::e1;

    _depth_stencil_info.depthTestEnable       = VK_FALSE;
    _depth_stencil_info.depthWriteEnable      = VK_FALSE;
    _depth_stencil_info.depthCompareOp        = vk::CompareOp::eLess;
    _depth_stencil_info.depthBoundsTestEnable = VK_FALSE;
    _depth_stencil_info.stencilTestEnable     = VK_FALSE;

    // 默认不混合
    disableColorBlending();

    // 默认动态状态
    _dynamic_states = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    _vertex_input_info.vertexBindingDescriptionCount   = 0;
    _vertex_input_info.vertexAttributeDescriptionCount = 0;
}

std::unique_ptr<Pipeline> PipelineBuilder::build()
{
    if (_pipeline_layout == nullptr)
    {
        throw std::runtime_error("Pipeline layout must be set before building a pipeline.");
    }
    if (_shader_stages.empty())
    {
        throw std::runtime_error("Shader stages must be set before building a pipeline.");
    }

    if (_type == PipelineType::Compute)
    {
        // ---- Compute pipeline 路径 ----
        if (_shader_stages.size() != 1 ||
            _shader_stages[0].stage != vk::ShaderStageFlagBits::eCompute)
        {
            throw std::runtime_error("Compute pipeline requires exactly one compute shader stage.");
        }

        vk::ComputePipelineCreateInfo cpInfo;
        cpInfo.stage  = _shader_stages[0];
        cpInfo.layout = _pipeline_layout->getHandle();

        auto result = _context->getDevice().createComputePipeline(nullptr, cpInfo);
        if (result.result != vk::Result::eSuccess)
        {
            throw std::runtime_error("Failed to create compute pipeline!");
        }

        return std::unique_ptr<Pipeline>(new Pipeline(_context, result.value, _pipeline_layout));
    }

    // ---- Graphics pipeline 路径（保持你原来的逻辑）----
    vk::PipelineViewportStateCreateInfo viewport_state_info;
    viewport_state_info.viewportCount = 1;
    viewport_state_info.scissorCount  = 1;

    vk::PipelineColorBlendStateCreateInfo color_blend_info;
    color_blend_info.logicOpEnable   = VK_FALSE;
    color_blend_info.attachmentCount = 1;
    color_blend_info.pAttachments    = &_color_blend_attachment;

    vk::PipelineDynamicStateCreateInfo dynamic_state_info({}, _dynamic_states);

    vk::GraphicsPipelineCreateInfo pipeline_info;
    pipeline_info.stageCount = static_cast<uint32_t>(_shader_stages.size());
    pipeline_info.pStages    = _shader_stages.data();
    // Mesh/Task Shader 管线：在 setShaderStages 时已判定
    const bool is_mesh_pipeline = _is_mesh_pipeline;

    vk::PipelineVertexInputStateCreateInfo empty_vertex_info;
    if (is_mesh_pipeline)
    {
        pipeline_info.pVertexInputState   = nullptr;
        pipeline_info.pInputAssemblyState = nullptr;
    }
    else
    {
        if (_use_vertex_input)
        {
            _vertex_input_info.vertexBindingDescriptionCount =
                static_cast<uint32_t>(_vertex_bindings.size());
            _vertex_input_info.pVertexBindingDescriptions = _vertex_bindings.data();
            _vertex_input_info.vertexAttributeDescriptionCount =
                static_cast<uint32_t>(_vertex_attributes.size());
            _vertex_input_info.pVertexAttributeDescriptions = _vertex_attributes.data();
            pipeline_info.pVertexInputState = &_vertex_input_info;
        }
        else
        {
            pipeline_info.pVertexInputState = &empty_vertex_info;
        }
        pipeline_info.pInputAssemblyState = &_input_assembly_info;
    }

    pipeline_info.pViewportState      = &viewport_state_info;
    pipeline_info.pRasterizationState = &_rasterization_info;
    pipeline_info.pMultisampleState   = &_multisample_info;
    pipeline_info.pDepthStencilState  = &_depth_stencil_info;
    pipeline_info.pColorBlendState    = &color_blend_info;
    pipeline_info.pDynamicState       = &dynamic_state_info;
    pipeline_info.layout              = _pipeline_layout->getHandle();

    vk::PipelineRenderingCreateInfo rendering_info;
    rendering_info.colorAttachmentCount    = static_cast<uint32_t>(_color_attachment_formats.size());
    rendering_info.pColorAttachmentFormats = _color_attachment_formats.data();
    rendering_info.depthAttachmentFormat   = _depth_attachment_format;
    pipeline_info.pNext                    = &rendering_info;

    auto result = _context->getDevice().createGraphicsPipeline(VK_NULL_HANDLE, pipeline_info);
    if (result.result != vk::Result::eSuccess)
    {
        throw std::runtime_error("Failed to create graphics pipeline!");
    }

    return std::unique_ptr<Pipeline>(new Pipeline(_context, result.value, _pipeline_layout));
}



// ---- 统一的 Stage 入口（C++23） ----
static bool isMeshLikeStage(vk::ShaderStageFlagBits stage)
{
    return stage == vk::ShaderStageFlagBits::eMeshEXT || stage == vk::ShaderStageFlagBits::eTaskEXT;
}

PipelineBuilder& PipelineBuilder::setShaderStages(std::span<const ShaderStageDesc> stages)
{
    if (stages.empty())
    {
        throw std::runtime_error("setShaderStages: empty stage list.");
    }

    if (stages.size() > _shader_stages.max_size())
    {
        throw std::runtime_error("setShaderStages: too many stages (increase inplace_vector capacity).");
    }

    // 判定管线类型 + 合法性检查
    bool has_compute   = false;
    bool has_graphics  = false;
    bool has_mesh_like = false;

    for (const auto& s : stages)
    {
        if (s.stage == vk::ShaderStageFlagBits::eCompute) has_compute = true;
        else has_graphics = true;

        if (isMeshLikeStage(s.stage)) has_mesh_like = true;
    }

    if (has_compute && has_graphics)
    {
        throw std::runtime_error("setShaderStages: cannot mix compute and graphics stages.");
    }

    _type             = has_compute ? PipelineType::Compute : PipelineType::Graphics;
    _is_mesh_pipeline = (!has_compute) && has_mesh_like;

    _shader_stages.clear();
    for (const auto& s : stages)
    {
        _shader_stages.push_back(vk::PipelineShaderStageCreateInfo{
            {},
            s.stage,
            s.module,
            s.entry.data(),
            s.specialization
        });
    }
    return *this;
}

PipelineBuilder& PipelineBuilder::setShaderStages(std::initializer_list<ShaderStageDesc> stages)
{
    return setShaderStages(std::span<const ShaderStageDesc>{stages.begin(), stages.size()});
}

// ---- 便捷封装（薄包装，避免重复） ----
PipelineBuilder& PipelineBuilder::setGraphicsShaders(vk::ShaderModule vertex_shader,
                                                     vk::ShaderModule fragment_shader,
                                                     std::string_view vs_entry,
                                                     std::string_view fs_entry)
{
    return setShaderStages({
        {vk::ShaderStageFlagBits::eVertex,   vertex_shader,   vs_entry},
        {vk::ShaderStageFlagBits::eFragment, fragment_shader, fs_entry},
    });
}

PipelineBuilder& PipelineBuilder::setMeshShaders(vk::ShaderModule mesh_shader,
                                                 vk::ShaderModule fragment_shader,
                                                 std::string_view ms_entry,
                                                 std::string_view fs_entry)
{
    return setShaderStages({
        {vk::ShaderStageFlagBits::eMeshEXT,  mesh_shader,     ms_entry},
        {vk::ShaderStageFlagBits::eFragment, fragment_shader, fs_entry},
    });
}

PipelineBuilder& PipelineBuilder::setTaskMeshShaders(vk::ShaderModule task_shader,
                                                     vk::ShaderModule mesh_shader,
                                                     vk::ShaderModule fragment_shader,
                                                     std::string_view ts_entry,
                                                     std::string_view ms_entry,
                                                     std::string_view fs_entry)
{
    return setShaderStages({
        {vk::ShaderStageFlagBits::eTaskEXT,  task_shader,     ts_entry},
        {vk::ShaderStageFlagBits::eMeshEXT,  mesh_shader,     ms_entry},
        {vk::ShaderStageFlagBits::eFragment, fragment_shader, fs_entry},
    });
}

PipelineBuilder& PipelineBuilder::setComputeShader(vk::ShaderModule comp_shader, std::string_view cs_entry)
{
    return setShaderStages({
        {vk::ShaderStageFlagBits::eCompute, comp_shader, cs_entry},
    });
}

PipelineBuilder& PipelineBuilder::setLayout(PipelineLayout* layout)
{
    _pipeline_layout = layout;
    return *this;
}

PipelineBuilder& PipelineBuilder::setRenderingInfo(const std::vector<vk::Format>& color_formats,
                                                   vk::Format                     depth_format)
{
    _color_attachment_formats = color_formats;
    _depth_attachment_format  = depth_format;
    return *this;
}

PipelineBuilder& PipelineBuilder::setPrimitiveTopology(vk::PrimitiveTopology topology, bool primitive_restart)
{
    _input_assembly_info.topology               = topology;
    _input_assembly_info.primitiveRestartEnable = primitive_restart ? VK_TRUE : VK_FALSE;
    return *this;
}

PipelineBuilder& PipelineBuilder::setPolygonMode(vk::PolygonMode mode)
{
    _rasterization_info.polygonMode = mode;
    return *this;
}

PipelineBuilder& PipelineBuilder::setCullMode(vk::CullModeFlags cull_mode, vk::FrontFace front_face)
{
    _rasterization_info.cullMode  = cull_mode;
    _rasterization_info.frontFace = front_face;
    return *this;
}

PipelineBuilder& PipelineBuilder::enableDepthTest(bool write_enable, vk::CompareOp op)
{
    _depth_stencil_info.depthTestEnable  = VK_TRUE;
    _depth_stencil_info.depthWriteEnable = write_enable;
    _depth_stencil_info.depthCompareOp   = op;
    return *this;
}

PipelineBuilder& PipelineBuilder::disableDepthTest()
{
    _depth_stencil_info.depthTestEnable  = VK_FALSE;
    _depth_stencil_info.depthWriteEnable = VK_FALSE;
    return *this;
}

PipelineBuilder& PipelineBuilder::setColorBlending(const vk::PipelineColorBlendAttachmentState& blend_attachment)
{
    _color_blend_attachment = blend_attachment;
    return *this;
}

PipelineBuilder& PipelineBuilder::disableColorBlending()
{
    _color_blend_attachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                                             vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    _color_blend_attachment.blendEnable = VK_FALSE;
    return *this;
}

PipelineBuilder& PipelineBuilder::setColorBlendingAdditive()
{
    _color_blend_attachment.blendEnable         = VK_TRUE;
    _color_blend_attachment.srcColorBlendFactor = vk::BlendFactor::eOne;
    _color_blend_attachment.dstColorBlendFactor = vk::BlendFactor::eOne;
    _color_blend_attachment.colorBlendOp        = vk::BlendOp::eAdd;
    _color_blend_attachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
    _color_blend_attachment.dstAlphaBlendFactor = vk::BlendFactor::eOne;
    _color_blend_attachment.alphaBlendOp        = vk::BlendOp::eAdd;
    _color_blend_attachment.colorWriteMask      = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                                             vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    return *this;
}

PipelineBuilder& PipelineBuilder::setColorBlendingAlpha()
{
    _color_blend_attachment.blendEnable         = VK_TRUE;
    _color_blend_attachment.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
    _color_blend_attachment.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
    _color_blend_attachment.colorBlendOp        = vk::BlendOp::eAdd;
    _color_blend_attachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
    _color_blend_attachment.dstAlphaBlendFactor = vk::BlendFactor::eZero;
    _color_blend_attachment.alphaBlendOp        = vk::BlendOp::eAdd;
    _color_blend_attachment.colorWriteMask      = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                                             vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    return *this;
}

PipelineBuilder& PipelineBuilder::addDynamicState(vk::DynamicState state)
{
    _dynamic_states.push_back(state);
    return *this;
}

PipelineBuilder& PipelineBuilder::setVertexInput(
    const std::vector<vk::VertexInputBindingDescription>& bindings,
    const std::vector<vk::VertexInputAttributeDescription>& attributes)
{
    _vertex_bindings   = bindings;
    _vertex_attributes = attributes;
    _use_vertex_input  = true;
    return *this;
}
} // namespace dk::vkcore
