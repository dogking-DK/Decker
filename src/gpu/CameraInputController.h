#pragma once

#include "camera.h"
#include "input/InputRouter.h"

namespace dk {

class CameraInputController final : public input::IInputConsumer
{
public:
    explicit CameraInputController(Camera& camera);

    bool handleInput(const input::InputState& state, const input::InputContext& ctx) override;

private:
    Camera& _camera;
};

} // namespace dk
