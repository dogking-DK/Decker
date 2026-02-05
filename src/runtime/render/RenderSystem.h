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
class BlitPass;
class GaussianBlurPass;
class DistortionPass;
struct ImageDesc;
class FrameGraphImage;
template <typename DescT, typename RealResT>
class RGResource;
}

namespace dk::render {
class OpaquePass;
class DebugAabbPass;
class OutlinePass;
class UiGizmoPass;

struct PostProcessSettings
{
    bool enable_distortion{false};
    bool enable_gaussian_blur{false};
};

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
    void executePostProcess(dk::RenderGraphContext& ctx,
                            VkImage                image,
                            VkImageView            image_view,
                            const vk::Extent3D&    extent,
                            vk::Format             format);

    void setFluidData(const FluidRenderData& data) { _fluid_data = data; }
    void setVoxelData(const VoxelRenderData& data) { _voxel_data = data; }
    void setDebugDrawAabb(bool enabled) { _debug_draw_aabb = enabled; }
    void setSelectedNodeId(const UUID& id) { _selected_node_id = id; }
    void beginUiFrame() { _ui_render_service.beginFrame(); }
    void finalizeUiFrame() { _ui_render_service.finalize(); }
    UiRenderService& uiRenderService() { return _ui_render_service; }
    const PostProcessSettings& postProcessSettings() const { return _postprocess_settings; }
    void setPostProcessSettings(const PostProcessSettings& settings);

    const FrameStats& stats() const { return _stats; }
    const RenderWorld& getRenderWorld() const { return _render_world; }
private:
    void buildDrawLists();
    void rebuildPostProcessGraph(VkImage             image,
                                 VkImageView         image_view,
                                 const vk::Extent3D& extent,
                                 vk::Format          format);

    vkcore::VulkanContext&             _context;
    ResourceLoader&                     _cpu_loader;
    RenderWorld                          _render_world;
    DebugRenderService                   _debug_render_service;
    UiRenderService                      _ui_render_service;
    DrawLists                            _draw_lists;
    FrameContext                         _frame_ctx;
    FrameStats                           _stats;
    std::unique_ptr<GpuResourceManager>  _gpu_cache;
    std::unique_ptr<OpaquePass>          _opaque_pass;
    std::unique_ptr<OutlinePass>         _outline_pass;
    std::unique_ptr<DebugAabbPass>       _debug_aabb_pass;
    std::unique_ptr<UiGizmoPass>         _ui_gizmo_pass;
    RenderGraph                          _graph;
    bool                                 _compiled{false};
    std::optional<FluidRenderData>       _fluid_data;
    std::optional<VoxelRenderData>       _voxel_data;
    bool                                 _debug_draw_aabb{false};
    UUID                                 _selected_node_id{};
    PostProcessSettings                  _postprocess_settings{};
    bool                                 _postprocess_dirty{false};
    std::shared_ptr<RenderGraph>         _postprocess_graph;
    std::shared_ptr<RGResource<ImageDesc, FrameGraphImage>> _postprocess_color_image;
    std::shared_ptr<BlitPass>            _postprocess_blit_pass;
    std::shared_ptr<GaussianBlurPass>    _gaussian_blur_pass;
    std::shared_ptr<DistortionPass>      _distortion_pass;
};
} // namespace dk::render
