#include "RenderSystem.h"

#include <algorithm>

#include "render graph/renderpass/OpaquePass.h"
#include "render/RenderTypes.h"

namespace dk::render {
RenderSystem::RenderSystem(vkcore::VulkanContext& ctx, vkcore::UploadContext& upload_ctx, ResourceLoader& loader)
    : _cpu_loader(loader)
{
    _gpu_cache   = std::make_unique<GpuResourceManager>(ctx, upload_ctx, loader);
    _opaque_pass = std::make_unique<OpaquePass>(ctx);
}

RenderSystem::~RenderSystem()
{
}

void RenderSystem::init(vk::Format color_format, vk::Format depth_format)
{
    _opaque_pass->init(color_format, depth_format);
    _gpu_cache->setMaterialDescriptorLayout(_opaque_pass->getMaterialDescriptorLayout());
    _opaque_pass->registerToGraph(_graph);

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

    _render_world.extractFromScene(scene, _cpu_loader, *_gpu_cache);
    buildDrawLists();

    _opaque_pass->setFrameData(&_frame_ctx, &_draw_lists);
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

    _stats.visible_proxies = _draw_lists.opaque.size();

    std::sort(_draw_lists.opaque.begin(), _draw_lists.opaque.end(),
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
