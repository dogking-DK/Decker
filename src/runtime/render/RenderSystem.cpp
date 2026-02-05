#include "RenderSystem.h"

#include <algorithm>

#include "BVH/Frustum.hpp"
#include "gpu/render graph/renderpass/DebugAabbPass.h"
#include "gpu/render graph/renderpass/OutlinePass.h"
#include "gpu/render graph/renderpass/UiGizmoPass.h"
#include "gpu/render graph/renderpass/BlitPass.h"
#include "gpu/render graph/renderpass/DistortionPass.h"
#include "gpu/render graph/renderpass/GaussianBlurPass.h"
#include "render graph/renderpass/OpaquePass.h"
#include "render graph/ResourceTexture.h"
#include "render/RenderTypes.h"

namespace dk::render {
RenderSystem::RenderSystem(vkcore::VulkanContext& ctx, vkcore::UploadContext& upload_ctx, ResourceLoader& loader)
    : _context(ctx)
    , _cpu_loader(loader)
    , _debug_render_service(ctx, upload_ctx)
    , _ui_render_service(ctx, upload_ctx)
{
    _gpu_cache   = std::make_unique<GpuResourceManager>(ctx, upload_ctx, loader);
    _opaque_pass = std::make_unique<OpaquePass>(ctx);
    _outline_pass = std::make_unique<OutlinePass>(ctx);
    _debug_aabb_pass = std::make_unique<DebugAabbPass>(ctx);
    _ui_gizmo_pass = std::make_unique<UiGizmoPass>(ctx);
}

RenderSystem::~RenderSystem()
{
}

void RenderSystem::init(vk::Format color_format, vk::Format depth_format)
{
    _opaque_pass->init(color_format, depth_format);
    _gpu_cache->setMaterialDescriptorLayout(_opaque_pass->getMaterialDescriptorLayout());
    _opaque_pass->registerToGraph(_graph);

    _outline_pass->init(color_format, depth_format);
    _outline_pass->registerToGraph(_graph);

    _debug_aabb_pass->init(color_format, depth_format);
    _debug_aabb_pass->registerToGraph(_graph);

    _ui_gizmo_pass->init(color_format, depth_format);
    _ui_gizmo_pass->registerToGraph(_graph);

    _graph.addTask<int>(
        "Fluid Volume Pass",
        [](int&, RenderTaskBuilder&) {},
        [](const int&, RenderGraphContext&)
        {
            // TODO: 接入体渲染/体素流体的 Raymarch 或参与光照的体积合成。
        });

    _graph.addTask<int>(
        "Voxel Pass",
        [](int&, RenderTaskBuilder&) {},
        [](const int&, RenderGraphContext&)
        {
            // TODO: 接入体素表面或 SDF 可视化。
        });

    _graph.compile();
    _compiled = true;
}

void RenderSystem::prepareFrame(const Scene& scene,
                                const glm::mat4& view,
                                const glm::mat4& proj,
                                const glm::vec3& camera_position,
                                const vk::Extent2D& viewport)
{
    _frame_ctx.view            = view;
    _frame_ctx.proj            = proj;
    _frame_ctx.viewproj        = proj * view;
    _frame_ctx.camera_position = camera_position;
    _frame_ctx.viewport        = viewport;

    _render_world.setSelectedNode(_selected_node_id);
    _render_world.extractFromScene(scene, _cpu_loader, *_gpu_cache);
    buildDrawLists();

    _opaque_pass->setFrameData(&_frame_ctx, &_draw_lists);
    _outline_pass->setFrameData(&_frame_ctx, &_draw_lists);

    _debug_render_service.setEnabled(_debug_draw_aabb);
    _debug_render_service.updateFromScene(scene);
    _debug_aabb_pass->setFrameData(&_frame_ctx, &_debug_render_service);

    _ui_gizmo_pass->setFrameData(&_frame_ctx, &_ui_render_service);
}

void RenderSystem::execute(dk::RenderGraphContext& ctx)
{
    if (!_compiled)
    {
        _graph.compile();
        _compiled = true;
    }
    _graph.execute(ctx);
}

void RenderSystem::setPostProcessSettings(const PostProcessSettings& settings)
{
    if (settings.enable_distortion != _postprocess_settings.enable_distortion ||
        settings.enable_gaussian_blur != _postprocess_settings.enable_gaussian_blur)
    {
        _postprocess_dirty = true;
    }
    _postprocess_settings = settings;
}

void RenderSystem::executePostProcess(dk::RenderGraphContext& ctx,
                                      VkImage                image,
                                      VkImageView            image_view,
                                      const vk::Extent3D&    extent,
                                      vk::Format             format)
{
    if (!_postprocess_settings.enable_distortion && !_postprocess_settings.enable_gaussian_blur)
    {
        return;
    }

    if (!_postprocess_graph || _postprocess_dirty)
    {
        rebuildPostProcessGraph(image, image_view, extent, format);
    }

    if (_postprocess_graph)
    {
        _postprocess_graph->execute(ctx);
    }
}

void RenderSystem::rebuildPostProcessGraph(VkImage             image,
                                           VkImageView         image_view,
                                           const vk::Extent3D& extent,
                                           vk::Format          format)
{
    using MyRes = RGResource<ImageDesc, FrameGraphImage>;

    struct CreateTempData
    {
        MyRes* distortion = nullptr;
        MyRes* blur       = nullptr;
    };

    _postprocess_graph = std::make_shared<RenderGraph>();
    if (!_postprocess_blit_pass)
    {
        _postprocess_blit_pass = std::make_shared<BlitPass>();
    }
    if (!_gaussian_blur_pass)
    {
        _gaussian_blur_pass = std::make_shared<GaussianBlurPass>();
        _gaussian_blur_pass->init(&_context);
    }
    if (!_distortion_pass)
    {
        _distortion_pass = std::make_shared<DistortionPass>();
        _distortion_pass->init(&_context);
    }

    ImageDesc desc{
        .width = extent.width,
        .height = extent.height,
        .depth = extent.depth,
        .format = format
    };
    _postprocess_color_image = std::make_shared<MyRes>("color src", desc, ResourceLifetime::External);
    _postprocess_color_image->setExternal(image, image_view);
    _postprocess_blit_pass->setSrc(_postprocess_color_image.get());

    MyRes* distortion_output = nullptr;
    MyRes* blur_output       = nullptr;
    _postprocess_graph->addTask<CreateTempData>(
        "Create postprocess temp data",
        [&](CreateTempData& data, RenderTaskBuilder& builder)
        {
            ImageDesc temp_desc{
                .width = extent.width,
                .height = extent.height,
                .format = format,
                .usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eColorAttachment |
                         vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage
            };
            if (_postprocess_settings.enable_distortion)
            {
                data.distortion   = builder.create<MyRes>("color_distortion", temp_desc);
                distortion_output = data.distortion;
            }
            if (_postprocess_settings.enable_gaussian_blur)
            {
                data.blur  = builder.create<MyRes>("color_after_blur", temp_desc);
                blur_output = data.blur;
            }
        },
        [](const CreateTempData&, dk::RenderGraphContext&) {}
    );

    MyRes* current_input = _postprocess_color_image.get();
    if (_postprocess_settings.enable_distortion && distortion_output)
    {
        _distortion_pass->registerToGraph(*_postprocess_graph, current_input, distortion_output);
        current_input = distortion_output;
    }
    if (_postprocess_settings.enable_gaussian_blur && blur_output)
    {
        _gaussian_blur_pass->registerToGraph(*_postprocess_graph, current_input, blur_output);
        current_input = blur_output;
    }
    if (current_input != _postprocess_color_image.get())
    {
        _postprocess_blit_pass->registerToGraph(*_postprocess_graph, current_input, _postprocess_color_image.get());
    }
    _postprocess_graph->compile();
    _postprocess_dirty = false;
}

void RenderSystem::buildDrawLists()
{
    _draw_lists.reset();

    const auto& proxies = _render_world.proxies();
    _draw_lists.opaque.reserve(proxies.size());

    const Frustum frustum = make_frustum(_frame_ctx.viewproj);

    _stats.total_proxies   = proxies.size();
    _stats.visible_proxies = 0;
    _stats.opaque_draws    = 0;

    for (size_t i = 0; i < proxies.size(); ++i)
    {
        const auto& proxy = proxies[i];
        if (!proxy.visible)
        {
            continue;
        }
        if (!proxy.gpu_mesh)
        {
            continue;
        }

        if (!frustum.contains(proxy.world_bounds))
        {
            continue;
        }

        DrawItem item{};
        item.proxy_index = static_cast<uint32_t>(i);
        item.mesh        = proxy.gpu_mesh;
        item.material    = proxy.gpu_material;
        item.world       = proxy.world_transform;
        const glm::vec4 view_pos = _frame_ctx.view * glm::vec4(proxy.world_bounds.center(), 1.0f);
        item.depth = view_pos.z;

        _draw_lists.opaque.push_back(item);
    }

    // 关键入口：RenderSystem 根据选中节点筛选描边绘制列表。
    if (const auto selected_index = _render_world.findProxyIndex(_selected_node_id))
    {
        if (*selected_index < proxies.size())
        {
            const auto& proxy = proxies[*selected_index];
            if (proxy.visible && proxy.gpu_mesh)
            {
                DrawItem outline_item{};
                outline_item.proxy_index = static_cast<uint32_t>(*selected_index);
                outline_item.mesh        = proxy.gpu_mesh;
                outline_item.material    = proxy.gpu_material;
                outline_item.world       = proxy.world_transform;
                outline_item.depth       = 0.0f;
                _draw_lists.outline.push_back(outline_item);
            }
        }
    }

    _stats.visible_proxies = _draw_lists.opaque.size();

    std::ranges::sort(_draw_lists.opaque,
                      [](const DrawItem& a, const DrawItem& b)
                      {
                          if (a.material.get() != b.material.get())
                          {
                              return a.material.get() < b.material.get();
                          }
                          return a.mesh.get() < b.mesh.get();
                      });

    _stats.opaque_draws = _draw_lists.opaque.size();
}
} // namespace dk::render
