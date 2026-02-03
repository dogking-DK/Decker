#include "ui/gizmo/TranslateGizmo.h"

#include <glm/vec3.hpp>

#include "SceneTypes.h"

namespace dk::ui {

bool TranslateGizmo::handleInput(const dk::input::InputState& state, const dk::input::InputContext& ctx)
{
    if (!ctx.selectedNode || !isEnabled())
    {
        _dragging = false;
        return false;
    }

    if (ctx.imguiWantsMouse)
    {
        return false;
    }

    const auto& mouse = state.mouse();

    if (mouse.leftPressed)
    {
        _dragging        = true;
        _dragStart       = mouse.position;
        _startTranslation = ctx.selectedNode->local_transform.translation;
        return true;
    }

    if (_dragging && mouse.leftDown)
    {
        glm::vec2 delta = mouse.position - _dragStart;
        float     scale = 0.01f;
        ctx.selectedNode->local_transform.translation = _startTranslation +
            glm::vec3(delta.x * scale, -delta.y * scale, 0.0f);
        return true;
    }

    if (_dragging && mouse.leftReleased)
    {
        _dragging = false;
        return true;
    }

    return false;
}

void TranslateGizmo::render(const GizmoContext& /*ctx*/)
{
}

} // namespace dk::ui
