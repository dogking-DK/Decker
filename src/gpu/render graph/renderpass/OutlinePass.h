#pragma once

#include <memory>

#include <glm/mat4x4.hpp>

#include "render graph/RenderGraph.h"
#include "render graph/RenderTaskBuilder.h"
#include "render/DrawList.h"
#include "Vulkan/Context.h"
#include "Vulkan/Pipeline.h"
#include "Vulkan/PipelineLayout.h"

namespace dk::render {
class OutlinePass
{
public:
    explicit OutlinePass(vkcore::VulkanContext& ctx);

    void init(vk::Format color_format, vk::Format depth_format);
    void registerToGraph(RenderGraph& graph);

    void setFrameData(const FrameContext* frame, const DrawLists* lists)
    {
        _frame_ctx = frame;
        _draw_lists = lists;
    }

private:
    struct OutlinePassData
    {
    };

    struct PushConstants
    {
        glm::mat4 viewproj;
        glm::mat4 model;
        glm::vec4 params;
    };

    void record(::dk::RenderGraphContext& ctx) const;

    vkcore::VulkanContext&              _context;
    std::unique_ptr<vkcore::PipelineLayout> _pipeline_layout;
    std::unique_ptr<vkcore::Pipeline>  _pipeline;
    const FrameContext*                _frame_ctx{nullptr};
    const DrawLists*                   _draw_lists{nullptr};
};
} // namespace dk::render
