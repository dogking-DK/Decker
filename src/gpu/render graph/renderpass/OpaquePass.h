#pragma once

#include <memory>

#include <glm/mat4x4.hpp>

#include "render graph/RenderGraph.h"
#include "render graph/RenderTaskBuilder.h"
#include "render/DrawList.h"
#include "resource/gpu/GPUMesh.h"
#include "Vulkan/Context.h"
#include "Vulkan/DescriptorSetLayout.h"
#include "Vulkan/Pipeline.h"
#include "Vulkan/PipelineLayout.h"

namespace dk::render {
class OpaquePass
{
public:
    explicit OpaquePass(vkcore::VulkanContext& ctx);

    void init(vk::Format color_format, vk::Format depth_format);
    void registerToGraph(RenderGraph& graph);
    vkcore::DescriptorSetLayout* getMaterialDescriptorLayout() const { return _material_set_layout.get(); }

    void setFrameData(const FrameContext* frame, const DrawLists* lists)
    {
        _frame_ctx = frame;
        _draw_lists = lists;
    }

private:
    struct OpaquePassData
    {
    };

    struct PushConstants
    {
        glm::mat4 viewproj;
        glm::mat4 model;
    };

    void record(::dk::RenderGraphContext& ctx) const;

    vkcore::VulkanContext&       _context;
    std::unique_ptr<vkcore::DescriptorSetLayout> _material_set_layout;
    std::unique_ptr<vkcore::PipelineLayout> _pipeline_layout;
    std::unique_ptr<vkcore::Pipeline>       _pipeline;
    const FrameContext*          _frame_ctx{nullptr};
    const DrawLists*             _draw_lists{nullptr};
};
} // namespace dk::render
