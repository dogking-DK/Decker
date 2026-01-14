#pragma once

#include <memory>
#include <optional>

#include <vulkan/vulkan.hpp>

#include "render/DrawList.h"
#include "render/RenderWorld.h"
#include "render/SimulationRenderData.h"
#include "render graph/RenderGraph.h"

namespace dk::vkcore {
class VulkanContext;
struct UploadContext;
}

namespace dk {
class RenderGraph;
}

namespace dk::render {
class OpaquePass;

class RenderSystem
{
public:
    RenderSystem(vkcore::VulkanContext& ctx, vkcore::UploadContext& upload_ctx, ResourceLoader& loader);

    void init(vk::Format color_format, vk::Format depth_format);

    void prepareFrame(const Scene& scene,
                      const glm::mat4& view,
                      const glm::mat4& proj,
                      const glm::vec3& camera_position,
                      const vk::Extent2D& viewport);

    void execute(dk::RenderGraphContext& ctx);

    void setFluidData(const FluidRenderData& data) { _fluid_data = data; }
    void setVoxelData(const VoxelRenderData& data) { _voxel_data = data; }

    const FrameStats& stats() const { return _stats; }

private:
    void buildDrawLists();

    ResourceLoader&                     _cpu_loader;
    RenderWorld                          _render_world;
    DrawLists                            _draw_lists;
    FrameContext                         _frame_ctx;
    FrameStats                           _stats;
    std::unique_ptr<GpuResourceManager>  _gpu_cache;
    std::unique_ptr<OpaquePass>          _opaque_pass;
    RenderGraph                          _graph;
    bool                                 _compiled{false};
    std::optional<FluidRenderData>       _fluid_data;
    std::optional<VoxelRenderData>       _voxel_data;
};
} // namespace dk::render
