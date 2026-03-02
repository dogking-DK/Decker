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

}

RenderPassRegistry createBuiltinRenderPassRegistry()
{
    RenderPassRegistry registry{};

    registerColorDepthPass(registry, "ClearTargets", true);
    registerColorDepthPass(registry, "ClearRenderTargets", true);
    registerColorDepthPass(registry, "OpaquePass");
    registerColorDepthPass(registry, "OutlinePass");
    registerColorDepthPass(registry, "DebugAabbPass");
    registerColorDepthPass(registry, "UiGizmoPass");
    registerColorDepthPass(registry, "FluidVolumePass", true);
    registerColorDepthPass(registry, "VoxelPass", true);

    return registry;
}

}
