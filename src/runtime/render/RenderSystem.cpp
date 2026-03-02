#include "RenderSystem.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <string_view>
#include <unordered_map>

#include "BVH/Frustum.hpp"
#include "gpu/vk_types.h"
#include "render graph/GraphAssetIO.h"
#include "render graph/BuiltinPassRegistry.h"
#include "render graph/GraphCompiler.h"
#include "gpu/render graph/renderpass/DebugAabbPass.h"
#include "gpu/render graph/renderpass/OutlinePass.h"
#include "gpu/render graph/renderpass/UiGizmoPass.h"
#include "render graph/renderpass/OpaquePass.h"
#include "render/RenderTypes.h"
#include "Vulkan/CommandBuffer.h"
#include "Vulkan/Texture.h"

namespace {
std::string normalize_node_type(std::string_view raw)
{
    std::string out;
    out.reserve(raw.size());
    for (const char ch : raw)
    {
        if (ch == ' ' || ch == '_' || ch == '-' || ch == '\t')
        {
            continue;
        }
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    return out;
}

const std::string* find_string_param(const dk::GraphNodeAsset* node, std::string_view name)
{
    if (!node)
    {
        return nullptr;
    }

    for (const auto& param : node->params)
    {
        if (param.name != name)
        {
            continue;
        }
        if (const auto* text = std::get_if<std::string>(&param.value))
        {
            return text;
        }
        return nullptr;
    }

    return nullptr;
}
}

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
    _rg_color_name = "scene_color";
    _rg_depth_name = "scene_depth";
    _named_image_resources.clear();
    _graph_built = false;
    _compiled = false;

    if (buildGraphFromAsset())
    {
        return;
    }

    _graph = RenderGraph{};
    _rg_color = nullptr;
    _rg_depth = nullptr;
    _rg_color_name = "scene_color";
    _rg_depth_name = "scene_depth";
    _named_image_resources.clear();
    _graph_built = false;
    _compiled = false;
    buildGraphLegacy();
}

void RenderSystem::addRenderTargetResourcesTask()
{
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
            _rg_color_name = "scene_color";
            _named_image_resources[_rg_color_name] = color_res;

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
            _rg_depth_name = "scene_depth";
            _named_image_resources[_rg_depth_name] = depth_res;
        },
        [](const TargetData&, RenderGraphContext&)
        {
        });
}

bool RenderSystem::addAssetImageResourcesTask(const GraphAsset& graph_asset)
{
    const auto resources = graph_asset.resources;

    _graph.addTask<int>(
        "Render Targets",
        [this, resources](int&, RenderTaskBuilder& builder)
        {
            for (const auto& resource : resources)
            {
                if (resource.kind != ResourceKind::Image)
                {
                    continue;
                }

                if (!std::holds_alternative<GraphImageDesc>(resource.payload))
                {
                    continue;
                }

                const auto image_payload = std::get<GraphImageDesc>(resource.payload);
                ImageDesc image_desc{};
                image_desc.width = image_payload.width;
                image_desc.height = image_payload.height;
                image_desc.depth = image_payload.depth;
                image_desc.mipLevels = image_payload.mipLevels;
                image_desc.arrayLayers = image_payload.arrayLayers;
                image_desc.samples = static_cast<vk::SampleCountFlagBits>(image_payload.samples);
                image_desc.aspectMask = static_cast<vk::ImageAspectFlags>(image_payload.aspectMask);
                if (image_payload.format != 0)
                {
                    image_desc.format = static_cast<vk::Format>(image_payload.format);
                }
                if (image_payload.usage != 0)
                {
                    image_desc.usage = static_cast<vk::ImageUsageFlags>(image_payload.usage);
                }

                const std::string external_key = resource.externalKey.empty() ? resource.name : resource.externalKey;
                const std::string normalized_key = normalize_node_type(external_key);

                std::shared_ptr<vkcore::TextureResource> external_texture{};
                if (resource.lifetime == ResourceLifetime::External)
                {
                    if (normalized_key == "scenecolor")
                    {
                        external_texture = _color_target;
                    }
                    else if (normalized_key == "scenedepth")
                    {
                        external_texture = _depth_target;
                    }
                    else
                    {
                        std::cerr << "[RenderSystem] Unsupported external image key: "
                            << external_key << " for resource \"" << resource.name << "\"\n";
                        continue;
                    }

                    if (external_texture)
                    {
                        const auto extent = external_texture->getExtent();
                        image_desc.width = extent.width;
                        image_desc.height = extent.height;
                        image_desc.depth = extent.depth;
                        image_desc.format = external_texture->getFormat();
                        if (normalized_key == "scenecolor")
                        {
                            image_desc.usage = vk::ImageUsageFlagBits::eColorAttachment |
                                vk::ImageUsageFlagBits::eTransferSrc |
                                vk::ImageUsageFlagBits::eStorage;
                            image_desc.aspectMask = vk::ImageAspectFlagBits::eColor;
                        }
                        else
                        {
                            image_desc.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;
                            image_desc.aspectMask = vk::ImageAspectFlagBits::eDepth;
                        }
                    }
                }

                auto* image_res = builder.create<RGResource<ImageDesc, FrameGraphImage>>(
                    resource.name,
                    image_desc,
                    resource.lifetime);

                if (resource.lifetime == ResourceLifetime::External)
                {
                    if (external_texture)
                    {
                        image_res->setExternal(external_texture);
                    }
                    else
                    {
                        continue;
                    }
                }

                _named_image_resources[resource.name] = image_res;

                if (normalized_key == "scenecolor")
                {
                    _rg_color = image_res;
                    _rg_color_name = resource.name;
                }
                else if (normalized_key == "scenedepth")
                {
                    _rg_depth = image_res;
                    _rg_depth_name = resource.name;
                }
            }
        },
        [](const int&, RenderGraphContext&)
        {
        });

    if (!_rg_color)
    {
        if (const auto it = _named_image_resources.find("scene_color"); it != _named_image_resources.end())
        {
            _rg_color = it->second;
            _rg_color_name = it->first;
        }
    }
    if (!_rg_depth)
    {
        if (const auto it = _named_image_resources.find("scene_depth"); it != _named_image_resources.end())
        {
            _rg_depth = it->second;
            _rg_depth_name = it->first;
        }
    }

    return !_named_image_resources.empty();
}

void RenderSystem::addClearTargetsTask(RGResource<ImageDesc, FrameGraphImage>* color,
                                       RGResource<ImageDesc, FrameGraphImage>* depth)
{
    struct ClearTargetsData
    {
        RGResource<ImageDesc, FrameGraphImage>* color{nullptr};
        RGResource<ImageDesc, FrameGraphImage>* depth{nullptr};
    };

    _graph.addTask<ClearTargetsData>(
        "Clear Render Targets",
        [color, depth](ClearTargetsData& data, RenderTaskBuilder& builder)
        {
            data.color = color;
            data.depth = depth;
            if (data.color)
            {
                builder.write(data.color, ResourceUsage::ColorAttachment);
            }
            if (data.depth)
            {
                builder.write(data.depth, ResourceUsage::DepthStencilAttachment);
            }
        },
        [](const ClearTargetsData& data, RenderGraphContext& ctx)
        {
            if (!ctx.frame_data || !ctx.frame_data->command_buffer_graphic)
            {
                return;
            }
            if (!data.color || !data.depth)
            {
                return;
            }

            auto* color_image = data.color->get();
            auto* depth_image = data.depth->get();
            if (!color_image || !depth_image)
            {
                return;
            }

            const auto& color_desc = data.color->desc();
            const vk::Rect2D render_area{
                {0, 0},
                {color_desc.width, color_desc.height}
            };

            vk::RenderingAttachmentInfo color_attachment{};
            color_attachment.imageView   = color_image->getVkImageView();
            color_attachment.imageLayout = vk::ImageLayout::eGeneral;
            color_attachment.loadOp      = vk::AttachmentLoadOp::eClear;
            color_attachment.storeOp     = vk::AttachmentStoreOp::eStore;
            color_attachment.clearValue  = vk::ClearValue{vk::ClearColorValue{std::array<float, 4>{1.0f, 1.0f, 1.0f, 1.0f}}};

            vk::RenderingAttachmentInfo depth_attachment{};
            depth_attachment.imageView   = depth_image->getVkImageView();
            depth_attachment.imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
            depth_attachment.loadOp      = vk::AttachmentLoadOp::eClear;
            depth_attachment.storeOp     = vk::AttachmentStoreOp::eStore;
            depth_attachment.clearValue  = vk::ClearValue{vk::ClearDepthStencilValue{1.0f, 0}};

            auto& command_buffer = *ctx.frame_data->command_buffer_graphic;
            command_buffer.beginRenderingColor(render_area, color_attachment, &depth_attachment);
            command_buffer.endRendering();
        });
}

bool RenderSystem::buildGraphFromAsset()
{
    namespace fs = std::filesystem;
    const fs::path asset_path = absolute(get_exe_dir() / "assets/config/render_graph.json");

    GraphAsset graph_asset{};
    std::string load_error;
    if (!loadGraphAssetFromJsonFile(asset_path, graph_asset, load_error))
    {
        std::cerr << "[RenderSystem] Graph asset load failed: " << load_error << "\n";
        return false;
    }

    CompiledGraphAsset compiled_asset{};
    std::string compile_error;
    const RenderPassRegistry pass_registry = createBuiltinRenderPassRegistry();
    if (!compileGraphAsset(graph_asset, compiled_asset, compile_error, &pass_registry))
    {
        std::cerr << "[RenderSystem] Graph asset compile failed: " << compile_error << "\n";
        return false;
    }

    std::unordered_map<GraphNodeId, const GraphNodeAsset*> node_by_id;
    node_by_id.reserve(graph_asset.nodes.size());
    for (const auto& node : graph_asset.nodes)
    {
        node_by_id.emplace(node.id, &node);
    }

    if (!addAssetImageResourcesTask(graph_asset))
    {
        std::cerr << "[RenderSystem] Graph asset has no usable image resources\n";
        return false;
    }

    if (!_rg_color || !_rg_depth)
    {
        std::cerr << "[RenderSystem] Graph asset must provide scene color/depth resources\n";
        return false;
    }

    using NodeOutputs = std::unordered_map<std::string, std::string>;
    std::unordered_map<GraphNodeId, NodeOutputs> node_outputs;
    node_outputs.reserve(compiled_asset.nodes.size());

    auto find_image = [this](const std::string& name) -> RGResource<ImageDesc, FrameGraphImage>*
    {
        const auto it = _named_image_resources.find(name);
        if (it == _named_image_resources.end())
        {
            return nullptr;
        }
        return it->second;
    };

    auto resolve_input_resource_name = [&node_outputs](
        const CompiledGraphNode& compiled_node,
        const GraphNodeAsset*    node_asset,
        std::string_view         pin_name,
        std::string_view         fallback_name) -> std::string
    {
        if (const auto bind_it = compiled_node.inputPins.find(std::string(pin_name));
            bind_it != compiled_node.inputPins.end())
        {
            const auto outputs_it = node_outputs.find(bind_it->second.sourceNode);
            if (outputs_it != node_outputs.end())
            {
                const auto source_pin_it = outputs_it->second.find(bind_it->second.sourcePin);
                if (source_pin_it != outputs_it->second.end())
                {
                    return source_pin_it->second;
                }
            }
        }

        if (const auto* direct = find_string_param(node_asset, pin_name))
        {
            return *direct;
        }

        std::string alias{pin_name};
        constexpr std::string_view input_suffix = "_in";
        if (alias.ends_with(input_suffix))
        {
            alias.resize(alias.size() - input_suffix.size());
        }
        if (const auto* by_alias = find_string_param(node_asset, alias))
        {
            return *by_alias;
        }

        return std::string{fallback_name};
    };

    bool clear_added = false;
    bool has_render_passes = false;

    for (const auto& compiled_node : compiled_asset.nodes)
    {
        const auto node_it = node_by_id.find(compiled_node.id);
        if (node_it == node_by_id.end())
        {
            continue;
        }

        const GraphNodeAsset* node_asset = node_it->second;
        const std::string normalized_type = normalize_node_type(compiled_node.type);
        std::string color_name = resolve_input_resource_name(compiled_node, node_asset, "color_in", _rg_color_name);
        std::string depth_name = resolve_input_resource_name(compiled_node, node_asset, "depth_in", _rg_depth_name);

        auto* color_res = find_image(color_name);
        auto* depth_res = find_image(depth_name);
        auto& outputs = node_outputs[compiled_node.id];

        if (normalized_type == "cleartargets" || normalized_type == "clearrendertargets")
        {
            if (!color_res)
            {
                color_res = _rg_color;
                color_name = _rg_color_name;
            }
            if (!depth_res)
            {
                depth_res = _rg_depth;
                depth_name = _rg_depth_name;
            }
            if (!color_res || !depth_res)
            {
                std::cerr << "[RenderSystem] ClearTargets missing input resources\n";
                continue;
            }

            addClearTargetsTask(color_res, depth_res);
            clear_added = true;
            outputs["color_out"] = color_name;
            outputs["depth_out"] = depth_name;
            continue;
        }

        if (normalized_type == "opaquepass")
        {
            if (!color_res || !depth_res)
            {
                std::cerr << "[RenderSystem] OpaquePass missing color/depth inputs\n";
                continue;
            }
            _opaque_pass->registerToGraph(_graph, color_res, depth_res);
            has_render_passes = true;
            outputs["color_out"] = color_name;
            outputs["depth_out"] = depth_name;
            continue;
        }

        if (normalized_type == "outlinepass")
        {
            if (!color_res || !depth_res)
            {
                std::cerr << "[RenderSystem] OutlinePass missing color/depth inputs\n";
                continue;
            }
            _outline_pass->registerToGraph(_graph, color_res, depth_res);
            has_render_passes = true;
            outputs["color_out"] = color_name;
            outputs["depth_out"] = depth_name;
            continue;
        }

        if (normalized_type == "debugaabbpass")
        {
            if (!color_res || !depth_res)
            {
                std::cerr << "[RenderSystem] DebugAabbPass missing color/depth inputs\n";
                continue;
            }
            _debug_aabb_pass->registerToGraph(_graph, color_res, depth_res);
            has_render_passes = true;
            outputs["color_out"] = color_name;
            outputs["depth_out"] = depth_name;
            continue;
        }

        if (normalized_type == "uigizmopass")
        {
            if (!color_res || !depth_res)
            {
                std::cerr << "[RenderSystem] UiGizmoPass missing color/depth inputs\n";
                continue;
            }
            _ui_gizmo_pass->registerToGraph(_graph, color_res, depth_res);
            has_render_passes = true;
            outputs["color_out"] = color_name;
            outputs["depth_out"] = depth_name;
            continue;
        }

        if (normalized_type == "fluidvolumepass")
        {
            _graph.addTask<int>(
                "Fluid Volume Pass",
                [](int&, RenderTaskBuilder&) {},
                [](const int&, RenderGraphContext&)
                {
                    // TODO: 接入体渲染/体素流体的 Raymarch 或参与光照的体积合成。
                });
            if (color_res) outputs["color_out"] = color_name;
            if (depth_res) outputs["depth_out"] = depth_name;
            continue;
        }

        if (normalized_type == "voxelpass")
        {
            _graph.addTask<int>(
                "Voxel Pass",
                [](int&, RenderTaskBuilder&) {},
                [](const int&, RenderGraphContext&)
                {
                    // TODO: 接入体素表面或 SDF 可视化。
                });
            if (color_res) outputs["color_out"] = color_name;
            if (depth_res) outputs["depth_out"] = depth_name;
            continue;
        }

        std::cerr << "[RenderSystem] Unknown graph node type: " << compiled_node.type
            << " (id=" << compiled_node.id << ")\n";
    }

    if (!clear_added)
    {
        addClearTargetsTask(_rg_color, _rg_depth);
    }
    if (!has_render_passes)
    {
        _opaque_pass->registerToGraph(_graph, _rg_color, _rg_depth);
    }

    _graph.compile();
    _compiled = true;
    _graph_built = true;
    return true;
}

void RenderSystem::buildGraphLegacy()
{
    addRenderTargetResourcesTask();
    addClearTargetsTask(_rg_color, _rg_depth);

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
