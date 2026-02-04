#pragma once

#include <memory>

#include <glm/mat4x4.hpp>

#include "render graph/RenderGraph.h"
#include "render graph/RenderTaskBuilder.h"
#include "render/DrawList.h"
#include "render/UiRenderService.h"
#include "Vulkan/Context.h"
#include "Vulkan/DescriptorSetLayout.h"
#include "Vulkan/Pipeline.h"
#include "Vulkan/PipelineLayout.h"

namespace dk::render {
class UiGizmoPass
{
public:
    explicit UiGizmoPass(vkcore::VulkanContext& ctx);

    void init(vk::Format color_format, vk::Format depth_format);
    void registerToGraph(RenderGraph& graph);

    void setFrameData(const FrameContext* frame, const UiRenderService* ui_service)
    {
        _frame_ctx = frame;
        _ui_service = ui_service;
    }

private:
    struct UiGizmoPassData
    {
    };

    struct PushConstants
    {
        glm::mat4 viewproj;
        glm::vec4 color;
    };

    void record(::dk::RenderGraphContext& ctx) const;

    vkcore::VulkanContext& _context;
    std::unique_ptr<vkcore::DescriptorSetLayout> _descriptor_set_layout;
    std::unique_ptr<vkcore::PipelineLayout> _pipeline_layout;
    std::unique_ptr<vkcore::Pipeline> _pipeline;
    const FrameContext* _frame_ctx{nullptr};
    const UiRenderService* _ui_service{nullptr};
};
} // namespace dk::render
