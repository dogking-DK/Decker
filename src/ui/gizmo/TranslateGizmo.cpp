#include "ui/gizmo/TranslateGizmo.h"

#include <glm/vec3.hpp>

#include "SceneTypes.h"

namespace dk::ui {

bool TranslateGizmo::handleInput(const dk::input::InputState& state, const dk::input::InputContext& ctx)
{
    if (!_state)
    {
        return false;
    }

    if (!ctx.selectedNode || !isEnabled())
    {
        _state->reset();
        return false;
    }

    if (ctx.imguiWantsMouse)
    {
        return false;
    }

    const auto& mouse = state.mouse();

    if (mouse.leftPressed)
    {
        _state->beginDrag(mouse.position, GizmoOperation::Translate, GizmoAxis::Screen);
        return true;
    }

    if (_state->dragging && mouse.leftDown)
    {
        glm::vec2 delta = mouse.position - _state->dragStart;
        float     scale = ctx.translateSensitivity;
        _state->updateDeltas(delta, glm::vec3(delta.x * scale, -delta.y * scale, 0.0f));
        return true;
    }

    if (_state->dragging && mouse.leftReleased)
    {
        glm::vec2 delta = mouse.position - _state->dragStart;
        float     scale = ctx.translateSensitivity;
        _state->updateDeltas(delta, glm::vec3(delta.x * scale, -delta.y * scale, 0.0f));
        _state->endDrag();
        return true;
    }

    return false;
}

void TranslateGizmo::render(const GizmoContext& /*ctx*/)
{
}

} // namespace dk::ui
