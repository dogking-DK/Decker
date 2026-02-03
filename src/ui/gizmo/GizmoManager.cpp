#include "ui/gizmo/GizmoManager.h"

namespace dk::ui {

void GizmoManager::registerGizmo(std::unique_ptr<IGizmo> gizmo)
{
    if (!gizmo)
    {
        return;
    }
    IGizmo* raw = gizmo.get();
    _gizmoMap[gizmo->toolType()] = raw;
    _gizmos.push_back(std::move(gizmo));
}

void GizmoManager::setActiveForTool(ToolType type)
{
    auto iter = _gizmoMap.find(type);
    _active = (iter != _gizmoMap.end()) ? iter->second : nullptr;
    for (auto& gizmo : _gizmos)
    {
        gizmo->setEnabled(gizmo.get() == _active);
    }
}

bool GizmoManager::handleInput(const dk::input::InputState& state, const dk::input::InputContext& ctx)
{
    if (!_active || !_active->isEnabled())
    {
        return false;
    }

    return _active->handleInput(state, ctx);
}

void GizmoManager::render(const GizmoContext& ctx)
{
    if (_active && _active->isEnabled())
    {
        _active->render(ctx);
    }
}

} // namespace dk::ui
