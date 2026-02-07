#pragma once

#include <memory>

#include <glm/mat4x4.hpp>

#include "render graph/RenderGraph.h"
#include "render graph/RenderTaskBuilder.h"
#include "render graph/ResourceTexture.h"
#include "render/DebugRenderService.h"
#include "render/DrawList.h"
#include "Vulkan/Context.h"
#include "Vulkan/DescriptorSetLayout.h"
#include "Vulkan/Pipeline.h"
#include "Vulkan/PipelineLayout.h"

namespace dk::render {
class DebugAabbPass
{
public:
    explicit DebugAabbPass(vkcore::VulkanContext& ctx);

    void init(vk::Format color_format, vk::Format depth_format);
    void registerToGraph(RenderGraph& graph,
                         RGResource<ImageDesc, FrameGraphImage>* color,
                         RGResource<ImageDesc, FrameGraphImage>* depth);

    void setFrameData(const FrameContext* frame, const DebugRenderService* debug_service)
    {
        _frame_ctx = frame;
        _debug_service = debug_service;
    }

private:
    struct DebugAabbPassData
    {
        RGResource<ImageDesc, FrameGraphImage>* color{nullptr};
        RGResource<ImageDesc, FrameGraphImage>* depth{nullptr};
    };

    struct PushConstants
    {
        glm::mat4 viewproj;
        glm::vec4 color;
    };

    void record(::dk::RenderGraphContext& ctx, const DebugAabbPassData& data) const;

    vkcore::VulkanContext& _context;
    std::unique_ptr<vkcore::DescriptorSetLayout> _descriptor_set_layout;
    std::unique_ptr<vkcore::PipelineLayout> _pipeline_layout;
    std::unique_ptr<vkcore::Pipeline> _pipeline;
    const FrameContext* _frame_ctx{nullptr};
    const DebugRenderService* _debug_service{nullptr};
};
} // namespace dk::render
