#include "BuiltinPassRegistry.h"

#include <utility>

namespace dk {
namespace {

PinSpec makeInputPin(std::string name, ResourceKind kind, ResourceUsage usage, bool optional = false)
{
    PinSpec pin{};
    pin.name = std::move(name);
    pin.direction = PinDirection::Input;
    pin.kind = kind;
    pin.defaultUsage = usage;
    pin.optional = optional;
    return pin;
}

PinSpec makeOutputPin(std::string name, ResourceKind kind, ResourceUsage usage, bool optional = false)
{
    PinSpec pin{};
    pin.name = std::move(name);
    pin.direction = PinDirection::Output;
    pin.kind = kind;
    pin.defaultUsage = usage;
    pin.optional = optional;
    return pin;
}

ParamSpec makeParam(std::string name, ParamType type)
{
    ParamSpec param{};
    param.name = std::move(name);
    param.type = type;
    param.optional = true;
    return param;
}

void registerColorDepthPass(RenderPassRegistry& registry, std::string type, bool inputs_optional = false)
{
    PassTypeInfo info{};
    info.type = std::move(type);
    info.displayName = info.type;
    info.queue = QueueClass::Graphics;
    info.pins = {
        makeInputPin("color_in", ResourceKind::Image, ResourceUsage::ColorAttachment, inputs_optional),
        makeInputPin("depth_in", ResourceKind::Image, ResourceUsage::DepthStencilAttachment, inputs_optional),
        makeOutputPin("color_out", ResourceKind::Image, ResourceUsage::ColorAttachment),
        makeOutputPin("depth_out", ResourceKind::Image, ResourceUsage::DepthStencilAttachment),
    };
    registry.registerType(std::move(info));
}

void registerClearTargetsPass(RenderPassRegistry& registry, std::string type)
{
    PassTypeInfo info{};
    info.type = std::move(type);
    info.displayName = info.type;
    info.queue = QueueClass::Graphics;
    info.pins = {
        makeInputPin("color_in", ResourceKind::Image, ResourceUsage::ColorAttachment, true),
        makeInputPin("depth_in", ResourceKind::Image, ResourceUsage::DepthStencilAttachment, true),
        makeOutputPin("color_out", ResourceKind::Image, ResourceUsage::ColorAttachment),
        makeOutputPin("depth_out", ResourceKind::Image, ResourceUsage::DepthStencilAttachment),
    };
    // Clear pass 支持参数驱动，便于脚本/编辑器配置清屏行为。
    info.params = {
        makeParam("clear_color", ParamType::Vec4),
        makeParam("clear_depth", ParamType::Float),
        makeParam("clear_stencil", ParamType::Int),
    };
    registry.registerType(std::move(info));
}

}

RenderPassRegistry createBuiltinRenderPassRegistry()
{
    RenderPassRegistry registry{};

    registerClearTargetsPass(registry, "ClearTargets");
    registerClearTargetsPass(registry, "ClearRenderTargets");
    registerColorDepthPass(registry, "OpaquePass");
    registerColorDepthPass(registry, "OutlinePass");
    registerColorDepthPass(registry, "DebugAabbPass");
    registerColorDepthPass(registry, "UiGizmoPass");
    registerColorDepthPass(registry, "FluidVolumePass", true);
    registerColorDepthPass(registry, "VoxelPass", true);

    return registry;
}

}
