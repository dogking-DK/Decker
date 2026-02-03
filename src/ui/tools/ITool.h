#pragma once

#include "input/InputRouter.h"
#include "ui/tools/ToolContext.h"
#include "ui/tools/ToolTypes.h"

namespace dk::ui {

class ITool : public dk::input::IInputConsumer
{
public:
    virtual ~ITool() = default;

    virtual void onActivate() {}
    virtual void onDeactivate() {}
    virtual void update(const ToolContext& ctx) {}

    virtual ToolType type() const = 0;
};

} // namespace dk::ui
