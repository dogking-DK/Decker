#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "ui/gizmo/GizmoDragState.h"
#include "ui/gizmo/IGizmo.h"

namespace dk::ui {

class RotateGizmo final : public IGizmo
{
public:
    explicit RotateGizmo(GizmoDragState* state) : _state(state) {}

    ToolType toolType() const override { return ToolType::Rotate; }

    bool handleInput(const dk::input::InputState& state, const dk::input::InputContext& ctx) override;
    void render(const GizmoContext& ctx) override;

    void setEnabled(bool enabled) override { _enabled = enabled; }
    bool isEnabled() const override { return _enabled; }

private:
    GizmoDragState* _state{nullptr};
    glm::vec3       _dragOrigin{};
    glm::vec3       _dragAxisWorld{};
    glm::vec3       _dragStartVectorWorld{};
    bool            _enabled{false};
};

} // namespace dk::ui
