#pragma once

#include <memory>
#include <vector>
#include "Resource.hpp"
#include "PipelineLayout.h"
#include "Context.h"

namespace dk::vkcore {
// 前向声明 Builder
class PipelineBuilder;

class Pipeline : public Resource<vk::Pipeline, vk::ObjectType::ePipeline>
{
public:
    // 禁止拷贝，允许移动
    Pipeline(const Pipeline&)            = delete;
    Pipeline& operator=(const Pipeline&) = delete;
    Pipeline(Pipeline&&)                 = default;
    Pipeline& operator=(Pipeline&&)      = default;

    ~Pipeline() override;

    PipelineLayout* getLayout() const { return _layout; }

private:
    // 构造函数私有，只能由 PipelineBuilder 创建
    friend class PipelineBuilder;
    Pipeline(VulkanContext* context, vk::Pipeline pipeline, PipelineLayout* layout);

    PipelineLayout* _layout; // 持有对其布局的引用
};


class PipelineBuilder
{
public:
    PipelineBuilder(VulkanContext* context);

    // 构建最终的 Pipeline 对象
    std::unique_ptr<Pipeline> build();

    // --- Fluent API for configuration ---

    // 设置着色器阶段
    PipelineBuilder& setShaders(vk::ShaderModule mesh_shader, vk::ShaderModule fragment_shader);
    PipelineBuilder& setShaders(vk::ShaderModule comp_shader);
    // 设置管线布局
    PipelineBuilder& setLayout(PipelineLayout* layout);

    // 设置渲染目标格式 (用于 Dynamic Rendering)
    PipelineBuilder& setRenderingInfo(const std::vector<vk::Format>& color_formats, vk::Format depth_format);

    // 设置光栅化状态
    PipelineBuilder& setPolygonMode(vk::PolygonMode mode);
    PipelineBuilder& setCullMode(vk::CullModeFlags cull_mode,
                                 vk::FrontFace     front_face = vk::FrontFace::eCounterClockwise);

    // 设置深度/模板状态
    PipelineBuilder& enableDepthTest(bool write_enable = true, vk::CompareOp op = vk::CompareOp::eLess);
    PipelineBuilder& disableDepthTest();

    // 设置颜色混合状态
    PipelineBuilder& setColorBlending(const vk::PipelineColorBlendAttachmentState& blend_attachment);
    PipelineBuilder& setColorBlendingAdditive();
    PipelineBuilder& setColorBlendingAlpha();
    PipelineBuilder& disableColorBlending();

    // 设置动态状态
    PipelineBuilder& addDynamicState(vk::DynamicState state);

private:
    VulkanContext* _context;

    // --- State storage ---
    PipelineLayout*                                _pipeline_layout{nullptr};
    std::vector<vk::PipelineShaderStageCreateInfo> _shader_stages;
    vk::PipelineInputAssemblyStateCreateInfo       _input_assembly_info;
    vk::PipelineRasterizationStateCreateInfo       _rasterization_info;
    vk::PipelineMultisampleStateCreateInfo         _multisample_info;
    vk::PipelineDepthStencilStateCreateInfo        _depth_stencil_info;
    vk::PipelineColorBlendAttachmentState          _color_blend_attachment;

    std::vector<vk::Format> _color_attachment_formats;
    vk::Format              _depth_attachment_format{vk::Format::eUndefined};

    std::vector<vk::DynamicState> _dynamic_states;

    enum class PipelineType { Graphics, Compute };
    PipelineType _type = PipelineType::Graphics;   // 新增：当前是图形还是计算
};
} // namespace dk::vkcore
