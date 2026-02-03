#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "ui/gizmo/IGizmo.h"

namespace dk::ui {

class TranslateGizmo final : public IGizmo
{
public:
    ToolType toolType() const override { return ToolType::Translate; }

    bool handleInput(const dk::input::InputState& state, const dk::input::InputContext& ctx) override;
    void render(const GizmoContext& ctx) override;

    void setEnabled(bool enabled) override { _enabled = enabled; }
    bool isEnabled() const override { return _enabled; }

private:
    bool      _enabled{false};
    bool      _dragging{false};
    glm::vec2 _dragStart{};
    glm::vec3 _startTranslation{};
};

} // namespace dk::ui
