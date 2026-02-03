#pragma once

#include <glm/vec3.hpp>

#include "ui/gizmo/GizmoDragState.h"
#include "ui/tools/ITool.h"

namespace dk::ui {

class TranslateTool final : public ITool
{
public:
    explicit TranslateTool(GizmoDragState* state) : _state(state) {}

    ToolType type() const override { return ToolType::Translate; }

    bool handleInput(const dk::input::InputState& state, const dk::input::InputContext& ctx) override;
    void update(const ToolContext& ctx) override;

private:
    GizmoDragState* _state{nullptr};
    glm::vec3 _startTranslation{};
};

} // namespace dk::ui
