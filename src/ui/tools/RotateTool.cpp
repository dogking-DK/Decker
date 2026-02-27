#include "ui/tools/RotateTool.h"

#include <glm/geometric.hpp>

#include "SceneTypes.h"

namespace dk::ui {
namespace {

glm::vec3 axisToLocalDirection(GizmoAxis axis)
{
    switch (axis)
    {
    case GizmoAxis::X:
        return {1.0f, 0.0f, 0.0f};
    case GizmoAxis::Y:
        return {0.0f, 1.0f, 0.0f};
    case GizmoAxis::Z:
        return {0.0f, 0.0f, 1.0f};
    default:
        return {0.0f, 0.0f, 0.0f};
    }
}

} // namespace

bool RotateTool::handleInput(const dk::input::InputState& /*state*/, const dk::input::InputContext& /*ctx*/)
{
    return false;
}

void RotateTool::update(const ToolContext& ctx)
{
    if (!_state || !ctx.rotateEnabled)
    {
        return;
    }

    if (!ctx.selectedNode)
    {
        _state->clearFrameFlags();
        return;
    }

    if (_state->operation != GizmoOperation::Rotate)
    {
        _state->clearFrameFlags();
        return;
    }

    if (_state->dragStarted)
    {
        _startRotation = ctx.selectedNode->local_transform.rotation;
    }

    if (_state->dragging)
    {
        const glm::vec3 axisLocal = axisToLocalDirection(_state->axis);
        if (glm::length(axisLocal) > 0.0f)
        {
            const glm::quat delta = glm::angleAxis(_state->rotationDelta, axisLocal);
            ctx.selectedNode->local_transform.rotation = glm::normalize(_startRotation * delta);
        }
    }

    _state->clearFrameFlags();
}

} // namespace dk::ui
