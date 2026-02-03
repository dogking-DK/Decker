#pragma once

#include "ui/tools/ITool.h"

namespace dk::ui {

class TranslateTool final : public ITool
{
public:
    ToolType type() const override { return ToolType::Translate; }

    bool handleInput(const dk::input::InputState& state, const dk::input::InputContext& ctx) override;
};

} // namespace dk::ui
