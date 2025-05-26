#include <camera.h>
#include <imgui.h>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp> // 使用 glm::value_ptr

#include "ControlPanel.h"

namespace dk {
Camera::Camera()
{
    projection = glm::perspective(glm::radians(60.f), 16.0f / 9.0f, 0.1f, 1500.0f);
    ortho      = glm::ortho(-640.0f, 640.0f, -360.0f, 360.0f, -500.0f, 500.0f);
}


void Camera::update()
{
    glm::mat4 cameraRotation = getRotationMatrix();

    position += glm::vec3(cameraRotation * glm::vec4(velocity, 0.f)) * velocity_coefficient;
}

glm::mat4 Camera::getViewMatrix()
{
    // to create a correct model view, we need to move the world in opposite
    // direction to the camera
    //  so we will create the camera model matrix and invert
    glm::mat4 cameraTranslation = translate(glm::mat4(1.f), position);
    glm::mat4 cameraRotation    = getRotationMatrix();
    return inverse(cameraTranslation * cameraRotation);
}

glm::mat4 Camera::getRotationMatrix()
{
    // fairly typical FPS style camera. we join the pitch and yaw rotations into
    // the final rotation matrix
    glm::quat pitchRotation = angleAxis(pitch, glm::vec3{1.f, 0.f, 0.f});
    glm::quat yawRotation   = angleAxis(yaw, glm::vec3{0.f, -1.f, 0.f});
    return toMat4(yawRotation) * toMat4(pitchRotation);
}

void Camera::processSDLEvent(SDL_Window* window, const SDL_Event& e)
{
    // 如果 ImGui 正在捕获鼠标事件，直接返回
    if (ImGui::GetIO().WantCaptureMouse)
        return;

    switch (e.type)
    {
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (e.button.button == SDL_BUTTON_LEFT)
        {
            SDL_SetWindowRelativeMouseMode(window, true);
            //SDL_SetRelativeMouseMode(SDL_TRUE);
            SDL_HideCursor();
            mouse_left_down = true;
        }
        break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (e.button.button == SDL_BUTTON_LEFT)
        {
            SDL_SetWindowRelativeMouseMode(window, false);
            //SDL_SetRelativeMouseMode(SDL_FALSE);
            //SDL_ShowCursor(SDL_ENABLE);
            SDL_ShowCursor();
            mouse_left_down = false;
        }
        break;
    case SDL_EVENT_KEY_DOWN:
        if (e.key.key == SDLK_W) { velocity.z = -1; }
        if (e.key.key == SDLK_S) { velocity.z = 1; }
        if (e.key.key == SDLK_A) { velocity.x = -1; }
        if (e.key.key == SDLK_D) { velocity.x = 1; }
        if (e.key.key == SDLK_LCTRL) { velocity.y = -1; }
        if (e.key.key == SDLK_SPACE) { velocity.y = 1; }
        break;
    case SDL_EVENT_KEY_UP:
        if (e.key.key == SDLK_W) { velocity.z = 0; }
        if (e.key.key == SDLK_S) { velocity.z = 0; }
        if (e.key.key == SDLK_A) { velocity.x = 0; }
        if (e.key.key == SDLK_D) { velocity.x = 0; }
        if (e.key.key == SDLK_LCTRL) { velocity.y = 0; }
        if (e.key.key == SDLK_SPACE) { velocity.y = 0; }
        break;
    case SDL_EVENT_MOUSE_MOTION:
        if (mouse_left_down)
        {
            yaw += static_cast<float>(e.motion.xrel) / 200.f;
            pitch -= static_cast<float>(e.motion.yrel) / 200.f;
        }
        break;
    default:
        break;
    }
}


void Camera::renderUI()
{
    if (ImGui::CollapsingHeader("相机设置", ImGuiTreeNodeFlags_DefaultOpen))
    {
        dk::ui::im_gui_blender_float("相机速度", &velocity_coefficient, 0.01f, 0.01f, 0.f, 100.f, "%.4f");
        ui::im_gui_blender_vec3("相机位置", position);
    }
}
} // dk
