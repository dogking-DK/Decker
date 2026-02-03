#pragma once

#include <memory>
#include <unordered_map>
#include <vector>

#include "ui/gizmo/IGizmo.h"

namespace dk::ui {

class GizmoManager : public dk::input::IInputConsumer
{
public:
    void registerGizmo(std::unique_ptr<IGizmo> gizmo);
    void setActiveForTool(ToolType type);
    IGizmo* activeGizmo() const { return _active; }

    bool handleInput(const dk::input::InputState& state, const dk::input::InputContext& ctx) override;
    void render(const GizmoContext& ctx);

private:
    std::vector<std::unique_ptr<IGizmo>> _gizmos;
    struct ToolTypeHash
    {
        size_t operator()(ToolType type) const noexcept { return static_cast<size_t>(type); }
    };

    std::unordered_map<ToolType, IGizmo*, ToolTypeHash> _gizmoMap;
    IGizmo*                               _active{nullptr};
};

} // namespace dk::ui
