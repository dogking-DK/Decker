#include "RenderSystem.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <string_view>
#include <unordered_map>
#include <utility>

#include <nlohmann/json.hpp>

#include "BVH/Frustum.hpp"
#include "gpu/vk_types.h"
#include "render graph/GraphAssetIO.h"
#include "render graph/GraphAssetViz.h"
#include "render graph/BuiltinPassRegistry.h"
#include "render graph/GraphCompiler.h"
#include "render graph/PassRegistryJson.h"
#include "gpu/render graph/renderpass/DebugAabbPass.h"
#include "gpu/render graph/renderpass/OutlinePass.h"
#include "gpu/render graph/renderpass/UiGizmoPass.h"
#include "render graph/renderpass/OpaquePass.h"
#include "render/RenderTypes.h"
#include "Vulkan/CommandBuffer.h"
#include "Vulkan/Texture.h"

namespace {
using ImageRGResource = dk::RGResource<dk::ImageDesc, dk::FrameGraphImage>;
using BufferRGResource = dk::RGResource<dk::BufferDesc, dk::FrameGraphBuffer>;
using ResolvedNodeResources = dk::render::RenderSystem::ResolvedNodeResources;
using NodeOutputs = dk::render::RenderSystem::NodeOutputs;
using json = nlohmann::json;

struct RenderGraphSourceConfig
{
    // Final graph JSON path consumed by GraphAssetIO.
    std::filesystem::path graphJsonPath{};
    // Optional working directory for generator command.
    std::filesystem::path workingDirectory{};
    // Optional pre-load command (for Python generation, etc.).
    std::string generatorCommand{};
};

std::filesystem::path resolvePathAgainst(const std::filesystem::path& base, const std::string& raw_path)
{
    if (raw_path.empty())
    {
        return {};
    }

    std::filesystem::path path{raw_path};
    if (path.is_absolute())
    {
        return path;
    }
    return base / path;
}

bool loadRenderGraphSourceConfig(const std::filesystem::path& exe_dir,
                                 RenderGraphSourceConfig& out_config,
                                 std::string& out_error)
{
    out_config = RenderGraphSourceConfig{};
    out_config.graphJsonPath = exe_dir / "assets/config/render_graph.json";
    out_config.workingDirectory = exe_dir;

    const auto config_path = exe_dir / "assets/config/render_graph_source.json";
    if (!std::filesystem::exists(config_path))
    {
        return true;
    }

    std::ifstream in(config_path, std::ios::binary);
    if (!in.is_open())
    {
        out_error = "Failed to open render graph source config: " + config_path.string();
        return false;
    }

    try
    {
        const json root = json::parse(in);
        if (const auto graph_json_it = root.find("graph_json");
            graph_json_it != root.end())
        {
            if (!graph_json_it->is_string())
            {
                out_error = "\"graph_json\" must be a string in " + config_path.string();
                return false;
            }
            out_config.graphJsonPath = resolvePathAgainst(exe_dir, graph_json_it->get<std::string>());
        }

        if (const auto command_it = root.find("generator_command");
            command_it != root.end())
        {
            if (!command_it->is_string())
            {
                out_error = "\"generator_command\" must be a string in " + config_path.string();
                return false;
            }
            out_config.generatorCommand = command_it->get<std::string>();
        }

        if (const auto working_directory_it = root.find("working_directory");
            working_directory_it != root.end())
        {
            if (!working_directory_it->is_string())
            {
                out_error = "\"working_directory\" must be a string in " + config_path.string();
                return false;
            }
            const std::string raw_working_directory = working_directory_it->get<std::string>();
            if (!raw_working_directory.empty())
            {
                out_config.workingDirectory = resolvePathAgainst(exe_dir, raw_working_directory);
            }
        }
    }
    catch (const std::exception& e)
    {
        out_error = "Failed to parse render graph source config: " + std::string(e.what());
        return false;
    }

    return true;
}

bool runRenderGraphGenerator(const RenderGraphSourceConfig& config, std::string& out_error)
{
    if (config.generatorCommand.empty())
    {
        return true;
    }

    if (!config.workingDirectory.empty() && !std::filesystem::exists(config.workingDirectory))
    {
        out_error = "Render graph generator working directory does not exist: " + config.workingDirectory.string();
        return false;
    }

    const std::string working_directory = config.workingDirectory.empty()
        ? std::filesystem::current_path().string()
        : config.workingDirectory.string();

#if defined(_WIN32)
    std::string command = "cmd /C \"cd /D \\\"" + working_directory + "\\\" && " + config.generatorCommand + "\"";
#else
    std::string command = "cd \"" + working_directory + "\" && " + config.generatorCommand;
#endif

    const int command_exit_code = std::system(command.c_str());
    if (command_exit_code != 0)
    {
        out_error = "Render graph generator command failed (exit=" + std::to_string(command_exit_code)
            + "): " + config.generatorCommand;
        return false;
    }

    return true;
}

std::string pin_alias_without_input_suffix(std::string_view pin_name)
{
    constexpr std::string_view input_suffix = "_in";
    if (pin_name.size() <= input_suffix.size() || !pin_name.ends_with(input_suffix))
    {
        return std::string{pin_name};
    }

    return std::string{pin_name.substr(0, pin_name.size() - input_suffix.size())};
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

template <typename T>
const T* find_typed_param(const dk::GraphNodeAsset* node, std::string_view name)
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
        return std::get_if<T>(&param.value);
    }

    return nullptr;
}

ImageRGResource* get_image_input(const ResolvedNodeResources& inputs, std::string_view pin_name)
{
    const auto it = inputs.inputImages.find(std::string(pin_name));
    if (it == inputs.inputImages.end())
    {
        return nullptr;
    }
    return it->second;
}

// 约定：若存在 x_in -> x_out 命名对，则自动做资源名透传。
void write_passthrough_outputs(const dk::PassTypeInfo& pass_info,
                               const ResolvedNodeResources& inputs,
                               NodeOutputs& outputs)
{
    constexpr std::string_view output_suffix = "_out";
    for (const auto& pin : pass_info.pins)
    {
        if (pin.direction != dk::PinDirection::Output)
        {
            continue;
        }

        std::string input_pin_name = pin.name;
        if (std::string_view pin_name_view{pin.name};
            pin_name_view.size() > output_suffix.size() && pin_name_view.ends_with(output_suffix))
        {
            input_pin_name = std::string{
                pin_name_view.substr(0, pin_name_view.size() - output_suffix.size())};
            input_pin_name += "_in";
        }

        const auto input_it = inputs.inputResourceNames.find(input_pin_name);
        if (input_it == inputs.inputResourceNames.end())
        {
            continue;
        }
        outputs[pin.name] = input_it->second;
    }
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
    registerBuiltinRuntimePassExecutors();
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

void RenderSystem::setExternalBufferResource(const std::string& external_key,
                                             const std::shared_ptr<vkcore::BufferResource>& buffer)
{
    const std::string normalized_key = RenderPassRegistry::normalizeTypeName(external_key);
    if (normalized_key.empty())
    {
        return;
    }

    if (buffer)
    {
        _external_buffer_resources[normalized_key] = buffer;
    }
    else
    {
        _external_buffer_resources.erase(normalized_key);
    }
}

void RenderSystem::registerRuntimePassExecutor(std::string_view pass_type,
                                               RuntimePassExecutor executor,
                                               bool contributes_render_pass)
{
    const std::string normalized_type = RenderPassRegistry::normalizeTypeName(pass_type);
    if (normalized_type.empty() || !executor)
    {
        return;
    }

    _runtime_pass_executors.insert_or_assign(
        normalized_type,
        RuntimePassExecutorEntry{
            .execute = std::move(executor),
            .contributesRenderPass = contributes_render_pass,
        });
}

void RenderSystem::registerBuiltinRuntimePassExecutors()
{
    _runtime_pass_executors.clear();

    const auto require_color_depth_inputs = [](std::string_view pass_name,
                                               const ResolvedNodeResources& inputs,
                                               ImageRGResource*& out_color,
                                               ImageRGResource*& out_depth,
                                               std::string& out_error) -> bool
    {
        out_color = get_image_input(inputs, "color_in");
        out_depth = get_image_input(inputs, "depth_in");
        if (out_color && out_depth)
        {
            return true;
        }

        out_error = std::string(pass_name) + " missing color/depth inputs";
        return false;
    };

    registerRuntimePassExecutor(
        "ClearTargets",
        [this](
            const CompiledGraphNode&,
            const GraphNodeAsset& node_asset,
            const PassTypeInfo& pass_info,
            ResolvedNodeResources& io_resources,
            NodeOutputs& io_outputs,
            std::string& out_error) -> bool
        {
            ImageRGResource* color = get_image_input(io_resources, "color_in");
            if (!color)
            {
                color = _rg_color;
                if (color)
                {
                    io_resources.inputImages["color_in"] = color;
                    io_resources.inputResourceNames["color_in"] = _rg_color_name;
                }
            }

            ImageRGResource* depth = get_image_input(io_resources, "depth_in");
            if (!depth)
            {
                depth = _rg_depth;
                if (depth)
                {
                    io_resources.inputImages["depth_in"] = depth;
                    io_resources.inputResourceNames["depth_in"] = _rg_depth_name;
                }
            }

            if (!color || !depth)
            {
                out_error = "ClearTargets missing color/depth inputs";
                return false;
            }

            std::array<float, 4> clear_color{1.0f, 1.0f, 1.0f, 1.0f};
            if (const auto* color_param = find_typed_param<std::array<float, 4>>(&node_asset, "clear_color"))
            {
                clear_color = *color_param;
            }

            float clear_depth = 1.0f;
            if (const auto* depth_param = find_typed_param<float>(&node_asset, "clear_depth"))
            {
                clear_depth = *depth_param;
            }

            std::uint32_t clear_stencil = 0;
            if (const auto* stencil_param = find_typed_param<std::int32_t>(&node_asset, "clear_stencil"))
            {
                clear_stencil = *stencil_param < 0 ? 0u : static_cast<std::uint32_t>(*stencil_param);
            }

            addClearTargetsTask(color, depth, clear_color, clear_depth, clear_stencil);
            write_passthrough_outputs(pass_info, io_resources, io_outputs);
            return true;
        });

    // ClearRenderTargets 是 ClearTargets 的别名，复用同一执行器。
    registerRuntimePassExecutor(
        "ClearRenderTargets",
        _runtime_pass_executors[RenderPassRegistry::normalizeTypeName("ClearTargets")].execute);

    const auto register_raster_pass_executor = [this, require_color_depth_inputs](
                                                   std::string_view type,
                                                   const std::function<void(ImageRGResource*, ImageRGResource*)>& register_fn,
                                                   std::string_view pass_name)
    {
        registerRuntimePassExecutor(
            type,
            [register_fn, pass_name = std::string(pass_name), require_color_depth_inputs](
                const CompiledGraphNode&,
                const GraphNodeAsset&,
                const PassTypeInfo& pass_info,
                ResolvedNodeResources& io_resources,
                NodeOutputs& io_outputs,
                std::string& out_error) -> bool
            {
                ImageRGResource* color = nullptr;
                ImageRGResource* depth = nullptr;
                if (!require_color_depth_inputs(pass_name, io_resources, color, depth, out_error))
                {
                    return false;
                }

                register_fn(color, depth);
                write_passthrough_outputs(pass_info, io_resources, io_outputs);
                return true;
            },
            true);
    };

    register_raster_pass_executor(
        "OpaquePass",
        [this](ImageRGResource* color, ImageRGResource* depth)
        {
            _opaque_pass->registerToGraph(_graph, color, depth);
        },
        "OpaquePass");

    register_raster_pass_executor(
        "OutlinePass",
        [this](ImageRGResource* color, ImageRGResource* depth)
        {
            _outline_pass->registerToGraph(_graph, color, depth);
        },
        "OutlinePass");

    register_raster_pass_executor(
        "DebugAabbPass",
        [this](ImageRGResource* color, ImageRGResource* depth)
        {
            _debug_aabb_pass->registerToGraph(_graph, color, depth);
        },
        "DebugAabbPass");

    register_raster_pass_executor(
        "UiGizmoPass",
        [this](ImageRGResource* color, ImageRGResource* depth)
        {
            _ui_gizmo_pass->registerToGraph(_graph, color, depth);
        },
        "UiGizmoPass");

    registerRuntimePassExecutor(
        "FluidVolumePass",
        [this](
            const CompiledGraphNode&,
            const GraphNodeAsset&,
            const PassTypeInfo& pass_info,
            ResolvedNodeResources& io_resources,
            NodeOutputs& io_outputs,
            std::string&) -> bool
        {
            _graph.addTask<int>(
                "Fluid Volume Pass",
                [](int&, RenderTaskBuilder&) {},
                [](const int&, RenderGraphContext&)
                {
                    // TODO: 接入体渲染/体素流体的 Raymarch 或参与光照的体积合成。
                });
            write_passthrough_outputs(pass_info, io_resources, io_outputs);
            return true;
        });

    registerRuntimePassExecutor(
        "VoxelPass",
        [this](
            const CompiledGraphNode&,
            const GraphNodeAsset&,
            const PassTypeInfo& pass_info,
            ResolvedNodeResources& io_resources,
            NodeOutputs& io_outputs,
            std::string&) -> bool
        {
            _graph.addTask<int>(
                "Voxel Pass",
                [](int&, RenderTaskBuilder&) {},
                [](const int&, RenderGraphContext&)
                {
                    // TODO: 接入体素表面或 SDF 可视化。
                });
            write_passthrough_outputs(pass_info, io_resources, io_outputs);
            return true;
        });
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
    _named_buffer_resources.clear();
    _graph_built = false;
    _compiled = false;
    _has_graph_asset_snapshot = false;

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
    _named_buffer_resources.clear();
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

bool RenderSystem::addAssetResourcesTask(const GraphAsset& graph_asset)
{
    const auto resources = graph_asset.resources;

    _graph.addTask<int>(
        "Render Targets",
        [this, resources](int&, RenderTaskBuilder& builder)
        {
            for (const auto& resource : resources)
            {
                if (resource.kind == ResourceKind::Image)
                {
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
                    const std::string normalized_key = RenderPassRegistry::normalizeTypeName(external_key);

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

                    continue;
                }

                if (resource.kind == ResourceKind::Buffer)
                {
                    if (!std::holds_alternative<GraphBufferDesc>(resource.payload))
                    {
                        continue;
                    }

                    const auto buffer_payload = std::get<GraphBufferDesc>(resource.payload);
                    BufferDesc buffer_desc{};
                    buffer_desc.size = buffer_payload.size;
                    if (buffer_payload.usage != 0)
                    {
                        buffer_desc.usage = static_cast<vk::BufferUsageFlags>(buffer_payload.usage);
                    }

                    if (resource.lifetime == ResourceLifetime::External)
                    {
                        const std::string external_key = resource.externalKey.empty() ? resource.name : resource.externalKey;
                        const std::string normalized_key = RenderPassRegistry::normalizeTypeName(external_key);
                        const auto external_it = _external_buffer_resources.find(normalized_key);
                        if (external_it == _external_buffer_resources.end() || !external_it->second)
                        {
                            std::cerr << "[RenderSystem] Missing external buffer binding: "
                                << external_key << " for resource \"" << resource.name << "\"\n";
                            continue;
                        }

                        auto* buffer_res = builder.create<RGResource<BufferDesc, FrameGraphBuffer>>(
                            resource.name,
                            buffer_desc,
                            resource.lifetime);
                        buffer_res->setExternal(external_it->second);
                        _named_buffer_resources[resource.name] = buffer_res;
                        continue;
                    }

                    auto* buffer_res = builder.create<RGResource<BufferDesc, FrameGraphBuffer>>(
                        resource.name,
                        buffer_desc,
                        resource.lifetime);
                    _named_buffer_resources[resource.name] = buffer_res;
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

    return !_named_image_resources.empty() || !_named_buffer_resources.empty();
}

void RenderSystem::addClearTargetsTask(RGResource<ImageDesc, FrameGraphImage>* color,
                                       RGResource<ImageDesc, FrameGraphImage>* depth,
                                       const std::array<float, 4>& clear_color,
                                       float clear_depth,
                                       std::uint32_t clear_stencil)
{
    struct ClearTargetsData
    {
        RGResource<ImageDesc, FrameGraphImage>* color{nullptr};
        RGResource<ImageDesc, FrameGraphImage>* depth{nullptr};
        std::array<float, 4> clearColor{1.0f, 1.0f, 1.0f, 1.0f};
        float clearDepth{1.0f};
        std::uint32_t clearStencil{0};
    };

    _graph.addTask<ClearTargetsData>(
        "Clear Render Targets",
        [color, depth, clear_color, clear_depth, clear_stencil](ClearTargetsData& data, RenderTaskBuilder& builder)
        {
            data.color = color;
            data.depth = depth;
            data.clearColor = clear_color;
            data.clearDepth = clear_depth;
            data.clearStencil = clear_stencil;
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
            color_attachment.clearValue  = vk::ClearValue{vk::ClearColorValue{data.clearColor}};

            vk::RenderingAttachmentInfo depth_attachment{};
            depth_attachment.imageView   = depth_image->getVkImageView();
            depth_attachment.imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
            depth_attachment.loadOp      = vk::AttachmentLoadOp::eClear;
            depth_attachment.storeOp     = vk::AttachmentStoreOp::eStore;
            depth_attachment.clearValue  = vk::ClearValue{vk::ClearDepthStencilValue{data.clearDepth, data.clearStencil}};

            auto& command_buffer = *ctx.frame_data->command_buffer_graphic;
            command_buffer.beginRenderingColor(render_area, color_attachment, &depth_attachment);
            command_buffer.endRendering();
        });
}

bool RenderSystem::buildGraphFromAsset()
{
    namespace fs = std::filesystem;
    const fs::path exe_dir = get_exe_dir();

    RenderGraphSourceConfig source_config{};
    std::string source_error;
    if (!loadRenderGraphSourceConfig(exe_dir, source_config, source_error))
    {
        std::cerr << "[RenderSystem] Graph source config load failed: " << source_error << "\n";
        return false;
    }

    if (!runRenderGraphGenerator(source_config, source_error))
    {
        std::cerr << "[RenderSystem] Graph source generator failed: " << source_error << "\n";
        return false;
    }

    const fs::path asset_path = absolute(source_config.graphJsonPath);

    GraphAsset graph_asset{};
    std::string load_error;
    if (!loadGraphAssetFromJsonFile(asset_path, graph_asset, load_error))
    {
        std::cerr << "[RenderSystem] Graph asset load failed: " << load_error << "\n";
        return false;
    }
    _graph_asset_snapshot = graph_asset;
    _has_graph_asset_snapshot = true;
    // Export runtime DOT for graph visualization tools (Graphviz, editor import, etc.).
    {
        std::string dot_error;
        const fs::path dot_path = absolute(get_exe_dir() / "assets/config/render_graph_runtime.dot");
        if (!saveGraphAssetToDotFile(graph_asset, dot_path, dot_error))
        {
            std::cerr << "[RenderSystem] Graph dot export failed: " << dot_error << "\n";
        }
    }

    CompiledGraphAsset compiled_asset{};
    std::string compile_error;
    const RenderPassRegistry pass_registry = createBuiltinRenderPassRegistry();

    // 导出 schema，便于可视化编辑器直接读取 pin/param 元数据。
    {
        std::string schema_error;
        const fs::path schema_path = absolute(get_exe_dir() / "assets/config/render_pass_schema.json");
        if (!savePassRegistrySchemaJsonFile(pass_registry, schema_path, schema_error))
        {
            std::cerr << "[RenderSystem] Pass schema export failed: " << schema_error << "\n";
        }
    }

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

    if (!addAssetResourcesTask(graph_asset))
    {
        std::cerr << "[RenderSystem] Graph asset has no usable resources\n";
        return false;
    }

    if (!_rg_color || !_rg_depth)
    {
        std::cerr << "[RenderSystem] Graph asset must provide scene color/depth resources\n";
        return false;
    }

    std::unordered_map<GraphNodeId, NodeOutputs> node_outputs;
    node_outputs.reserve(compiled_asset.nodes.size());

    auto find_image = [this](const std::string& name) -> ImageRGResource*
    {
        const auto it = _named_image_resources.find(name);
        if (it == _named_image_resources.end())
        {
            return nullptr;
        }
        return it->second;
    };

    auto find_buffer = [this](const std::string& name) -> BufferRGResource*
    {
        const auto it = _named_buffer_resources.find(name);
        if (it == _named_buffer_resources.end())
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

        const std::string alias = pin_alias_without_input_suffix(pin_name);
        if (const auto* by_alias = find_string_param(node_asset, alias))
        {
            return *by_alias;
        }

        return std::string{fallback_name};
    };

    auto default_fallback_for_pin = [this](std::string_view pin_name) -> std::string
    {
        if (pin_name == "color_in")
        {
            return _rg_color_name;
        }
        if (pin_name == "depth_in")
        {
            return _rg_depth_name;
        }
        return {};
    };

    bool clear_added = false;
    bool has_render_passes = false;
    if (_runtime_pass_executors.empty())
    {
        registerBuiltinRuntimePassExecutors();
    }
    const std::string clear_targets_type = RenderPassRegistry::normalizeTypeName("ClearTargets");
    const std::string clear_render_targets_type = RenderPassRegistry::normalizeTypeName("ClearRenderTargets");

    for (const auto& compiled_node : compiled_asset.nodes)
    {
        const auto node_it = node_by_id.find(compiled_node.id);
        if (node_it == node_by_id.end())
        {
            continue;
        }

        const GraphNodeAsset* node_asset = node_it->second;
        auto& outputs = node_outputs[compiled_node.id];

        const auto* pass_info = pass_registry.find(compiled_node.type);
        if (!pass_info)
        {
            std::cerr << "[RenderSystem] Missing pass schema for node type: " << compiled_node.type
                << " (id=" << compiled_node.id << ")\n";
            continue;
        }

        ResolvedNodeResources inputs{};
        bool missing_required_resource = false;
        for (const auto& pin : pass_info->pins)
        {
            if (pin.direction != PinDirection::Input)
            {
                continue;
            }

            const std::string fallback_name = default_fallback_for_pin(pin.name);
            const std::string resource_name = resolve_input_resource_name(
                compiled_node,
                node_asset,
                pin.name,
                fallback_name);
            if (resource_name.empty())
            {
                continue;
            }

            inputs.inputResourceNames.emplace(pin.name, resource_name);
            bool resolved = false;
            if (pin.kind == ResourceKind::Image)
            {
                if (auto* image = find_image(resource_name))
                {
                    inputs.inputImages.emplace(pin.name, image);
                    resolved = true;
                }
            }
            else if (pin.kind == ResourceKind::Buffer)
            {
                if (auto* buffer = find_buffer(resource_name))
                {
                    inputs.inputBuffers.emplace(pin.name, buffer);
                    resolved = true;
                }
            }
            else
            {
                if (auto* image = find_image(resource_name))
                {
                    inputs.inputImages.emplace(pin.name, image);
                    resolved = true;
                }
                else if (auto* buffer = find_buffer(resource_name))
                {
                    inputs.inputBuffers.emplace(pin.name, buffer);
                    resolved = true;
                }
            }

            if (!resolved && !pin.optional)
            {
                std::cerr << "[RenderSystem] Node " << compiled_node.id << " missing bound resource \""
                    << resource_name << "\" for pin \"" << pin.name << "\"\n";
                missing_required_resource = true;
            }
        }
        if (missing_required_resource)
        {
            continue;
        }

        const std::string normalized_type = RenderPassRegistry::normalizeTypeName(compiled_node.type);
        const auto executor_it = _runtime_pass_executors.find(normalized_type);
        if (executor_it == _runtime_pass_executors.end())
        {
            std::cerr << "[RenderSystem] Unknown graph node type: " << compiled_node.type
                << " (id=" << compiled_node.id << ")\n";
            continue;
        }

        std::string execute_error;
        if (!executor_it->second.execute(compiled_node, *node_asset, *pass_info, inputs, outputs, execute_error))
        {
            if (!execute_error.empty())
            {
                std::cerr << "[RenderSystem] Node execution skipped: " << execute_error
                    << " (type=" << compiled_node.type << ", id=" << compiled_node.id << ")\n";
            }
            continue;
        }

        if (executor_it->second.contributesRenderPass)
        {
            has_render_passes = true;
        }
        if (normalized_type == clear_targets_type || normalized_type == clear_render_targets_type)
        {
            clear_added = true;
        }
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
    _has_graph_asset_snapshot = false;
    _graph_asset_snapshot = GraphAsset{};
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
