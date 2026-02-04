#pragma once

#include <memory>
#include <optional>

#include <vulkan/vulkan.hpp>

#include "render/DrawList.h"
#include "render/DebugRenderService.h"
#include "render/RenderWorld.h"
#include "render/UiRenderService.h"
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
class DebugAabbPass;
class UiGizmoPass;

class RenderSystem
{
public:
    RenderSystem(vkcore::VulkanContext& ctx, vkcore::UploadContext& upload_ctx, ResourceLoader& loader);
    ~RenderSystem();
    void init(vk::Format color_format, vk::Format depth_format);

    void prepareFrame(const Scene& scene,
                      const glm::mat4& view,
                      const glm::mat4& proj,
                      const glm::vec3& camera_position,
                      const vk::Extent2D& viewport);

    void execute(dk::RenderGraphContext& ctx);

    void setFluidData(const FluidRenderData& data) { _fluid_data = data; }
    void setVoxelData(const VoxelRenderData& data) { _voxel_data = data; }
    void setDebugDrawAabb(bool enabled) { _debug_draw_aabb = enabled; }
    void beginUiFrame() { _ui_render_service.beginFrame(); }
    void finalizeUiFrame() { _ui_render_service.finalize(); }
    UiRenderService& uiRenderService() { return _ui_render_service; }

    const FrameStats& stats() const { return _stats; }
    const RenderWorld& getRenderWorld() const { return _render_world; }
private:
    void buildDrawLists();

    ResourceLoader&                     _cpu_loader;
    RenderWorld                          _render_world;
    DebugRenderService                   _debug_render_service;
    UiRenderService                      _ui_render_service;
    DrawLists                            _draw_lists;
    FrameContext                         _frame_ctx;
    FrameStats                           _stats;
    std::unique_ptr<GpuResourceManager>  _gpu_cache;
    std::unique_ptr<OpaquePass>          _opaque_pass;
    std::unique_ptr<DebugAabbPass>       _debug_aabb_pass;
    std::unique_ptr<UiGizmoPass>         _ui_gizmo_pass;
    RenderGraph                          _graph;
    bool                                 _compiled{false};
    std::optional<FluidRenderData>       _fluid_data;
    std::optional<VoxelRenderData>       _voxel_data;
    bool                                 _debug_draw_aabb{false};
};
} // namespace dk::render
