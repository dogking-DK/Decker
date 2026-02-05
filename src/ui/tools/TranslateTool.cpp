#include "ui/tools/TranslateTool.h"

#include "SceneTypes.h"

namespace dk::ui {

bool TranslateTool::handleInput(const dk::input::InputState& /*state*/, const dk::input::InputContext& /*ctx*/)
{
    return false;
}

void TranslateTool::update(const ToolContext& ctx)
{
    if (!_state || !ctx.translateEnabled)
    {
        return;
    }

    if (!ctx.selectedNode)
    {
        _state->clearFrameFlags();
        return;
    }

    if (_state->operation != GizmoOperation::Translate)
    {
        _state->clearFrameFlags();
        return;
    }

    if (_state->dragStarted)
    {
        _startTranslation = ctx.selectedNode->local_transform.translation;
    }

    if (_state->dragging || _state->dragEnded)
    {
        glm::vec3 delta = _state->worldDelta;
        switch (_state->axis)
        {
        case GizmoAxis::X:
            delta = glm::vec3(delta.x, 0.0f, 0.0f);
            break;
        case GizmoAxis::Y:
            delta = glm::vec3(0.0f, delta.y, 0.0f);
            break;
        case GizmoAxis::Z:
            delta = glm::vec3(0.0f, 0.0f, delta.z);
            break;
        case GizmoAxis::Screen:
        case GizmoAxis::None:
        case GizmoAxis::XY:
        case GizmoAxis::YZ:
        case GizmoAxis::ZX:
        default:
            break;
        }

        ctx.selectedNode->local_transform.translation = _startTranslation + delta;
    }

    _state->clearFrameFlags();
}

} // namespace dk::ui
