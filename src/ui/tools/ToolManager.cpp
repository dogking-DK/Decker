#include "ui/tools/ToolManager.h"

namespace dk::ui {

void ToolManager::registerTool(std::unique_ptr<ITool> tool)
{
    if (!tool)
    {
        return;
    }
    ToolType type = tool->type();
    _tools[type] = std::move(tool);
}

void ToolManager::setActiveTool(ToolType type)
{
    if (_activeType == type)
    {
        return;
    }

    if (_active)
    {
        _active->onDeactivate();
    }

    _activeType = type;
    auto        iter = _tools.find(type);
    _active = (iter != _tools.end()) ? iter->second.get() : nullptr;

    if (_active)
    {
        _active->onActivate();
    }
}

bool ToolManager::handleInput(const dk::input::InputState& state, const dk::input::InputContext& ctx)
{
    if (!_active)
    {
        return false;
    }
    return _active->handleInput(state, ctx);
}

void ToolManager::update(const ToolContext& ctx)
{
    if (_active)
    {
        _active->update(ctx);
    }
}

} // namespace dk::ui
