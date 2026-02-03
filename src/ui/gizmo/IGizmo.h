#pragma once

#include "input/InputRouter.h"
#include "ui/gizmo/GizmoContext.h"
#include "ui/tools/ToolTypes.h"

namespace dk::ui {

class IGizmo : public dk::input::IInputConsumer
{
public:
    virtual ~IGizmo() = default;

    virtual ToolType toolType() const = 0;

    virtual void render(const GizmoContext& ctx) = 0;

    virtual void setEnabled(bool enabled) = 0;
    virtual bool isEnabled() const = 0;
};

} // namespace dk::ui
