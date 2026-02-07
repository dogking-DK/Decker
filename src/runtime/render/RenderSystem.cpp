#include "RenderSystem.h"

#include <algorithm>

#include "BVH/Frustum.hpp"
#include "gpu/render graph/renderpass/DebugAabbPass.h"
#include "gpu/render graph/renderpass/OutlinePass.h"
#include "gpu/render graph/renderpass/UiGizmoPass.h"
#include "render graph/renderpass/OpaquePass.h"
#include "render/RenderTypes.h"
#include "Vulkan/Texture.h"

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
    _color_format = color_format;
    _depth_format = depth_format;

    _opaque_pass->init(color_format, depth_format);
    _gpu_cache->setMaterialDescriptorLayout(_opaque_pass->getMaterialDescriptorLayout());

    _outline_pass->init(color_format, depth_format);

    _debug_aabb_pass->init(color_format, depth_format);

    _ui_gizmo_pass->init(color_format, depth_format);
}

void RenderSystem::resizeRenderTargets(const vk::Extent2D& extent)
{
    if (extent.width == 0 || extent.height == 0)
    {
        return;
    }

    if (_target_extent.width == extent.width && _target_extent.height == extent.height &&
        _color_target && _depth_target)
    {
        return;
    }

    _target_extent = extent;

    vkcore::TextureResource::Builder color_builder;
    color_builder.setFormat(_color_format)
                 .setExtent({extent.width, extent.height, 1})
                 .setUsage(vk::ImageUsageFlagBits::eTransferSrc |
                           vk::ImageUsageFlagBits::eStorage |
                           vk::ImageUsageFlagBits::eColorAttachment)
                 .withVmaUsage(VMA_MEMORY_USAGE_AUTO);

    vkcore::TextureViewDesc color_view{};
    color_view.format = _color_format;
    color_view.aspectMask = vk::ImageAspectFlagBits::eColor;
    color_builder.withDefaultView(color_view);

    _color_target = color_builder.buildShared(_context);

    vkcore::TextureResource::Builder depth_builder;
    depth_builder.setFormat(_depth_format)
                 .setExtent({extent.width, extent.height, 1})
                 .setUsage(vk::ImageUsageFlagBits::eDepthStencilAttachment)
                 .withVmaUsage(VMA_MEMORY_USAGE_AUTO);

    vkcore::TextureViewDesc depth_view{};
    depth_view.format = _depth_format;
    depth_view.aspectMask = vk::ImageAspectFlagBits::eDepth;
    depth_builder.withDefaultView(depth_view);

    _depth_target = depth_builder.buildShared(_context);

    buildGraph();
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
    if (!_graph_built)
    {
        if (_color_target && _depth_target)
        {
            buildGraph();
        }
        else
        {
            return;
        }
    }
    if (!_compiled)
    {
        _graph.compile();
        _compiled = true;
    }
    _graph.execute(ctx);
}

void RenderSystem::buildGraph()
{
    if (!_color_target || !_depth_target)
    {
        return;
    }

    _graph = RenderGraph{};
    _rg_color = nullptr;
    _rg_depth = nullptr;
    _graph_built = false;
    _compiled = false;

    struct TargetData
    {
    };

    _graph.addTask<TargetData>(
        "Render Targets",
        [this](TargetData&, RenderTaskBuilder& builder)
        {
            const auto color_extent = _color_target->getExtent();
            ImageDesc color_desc{};
            color_desc.width = color_extent.width;
            color_desc.height = color_extent.height;
            color_desc.depth = color_extent.depth;
            color_desc.format = _color_target->getFormat();
            color_desc.usage = vk::ImageUsageFlagBits::eColorAttachment |
                               vk::ImageUsageFlagBits::eTransferSrc |
                               vk::ImageUsageFlagBits::eStorage;
            color_desc.aspectMask = vk::ImageAspectFlagBits::eColor;

            auto* color_res = builder.create<RGResource<ImageDesc, FrameGraphImage>>(
                "scene_color",
                color_desc,
                ResourceLifetime::External);
            color_res->setExternal(_color_target);
            _rg_color = color_res;

            const auto depth_extent = _depth_target->getExtent();
            ImageDesc depth_desc{};
            depth_desc.width = depth_extent.width;
            depth_desc.height = depth_extent.height;
            depth_desc.depth = depth_extent.depth;
            depth_desc.format = _depth_target->getFormat();
            depth_desc.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;
            depth_desc.aspectMask = vk::ImageAspectFlagBits::eDepth;

            auto* depth_res = builder.create<RGResource<ImageDesc, FrameGraphImage>>(
                "scene_depth",
                depth_desc,
                ResourceLifetime::External);
            depth_res->setExternal(_depth_target);
            _rg_depth = depth_res;
        },
        [](const TargetData&, RenderGraphContext&)
        {
        });

    _opaque_pass->registerToGraph(_graph, _rg_color, _rg_depth);
    _outline_pass->registerToGraph(_graph, _rg_color, _rg_depth);
    _debug_aabb_pass->registerToGraph(_graph, _rg_color, _rg_depth);
    _ui_gizmo_pass->registerToGraph(_graph, _rg_color, _rg_depth);

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
    _graph_built = true;
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
