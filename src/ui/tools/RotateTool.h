#pragma once

#include <glm/gtc/quaternion.hpp>

#include "ui/gizmo/GizmoDragState.h"
#include "ui/tools/ITool.h"

namespace dk::ui {

class RotateTool final : public ITool
{
public:
    explicit RotateTool(GizmoDragState* state) : _state(state) {}

    ToolType type() const override { return ToolType::Rotate; }

    bool handleInput(const dk::input::InputState& state, const dk::input::InputContext& ctx) override;
    void update(const ToolContext& ctx) override;

private:
    GizmoDragState* _state{nullptr};
    glm::quat       _startRotation{1.0f, 0.0f, 0.0f, 0.0f};
};

} // namespace dk::ui
