#pragma once

#include <memory>
#include <unordered_map>

#include "ui/tools/ITool.h"

namespace dk::ui {

class ToolManager : public dk::input::IInputConsumer
{
public:
    void registerTool(std::unique_ptr<ITool> tool);
    void setActiveTool(ToolType type);
    ToolType activeType() const { return _activeType; }

    ITool* activeTool() const { return _active; }

    bool handleInput(const dk::input::InputState& state, const dk::input::InputContext& ctx) override;
    void update(const ToolContext& ctx);

private:
    struct ToolTypeHash
    {
        size_t operator()(ToolType type) const noexcept { return static_cast<size_t>(type); }
    };

    std::unordered_map<ToolType, std::unique_ptr<ITool>, ToolTypeHash> _tools;
    ITool*                                               _active{nullptr};
    ToolType                                             _activeType{ToolType::None};
};

} // namespace dk::ui
