#define GLM_ENABLE_EXPERIMENTAL

#include <camera.h>
#include <imgui.h>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>

namespace dk {

Camera::Camera()
{
	projection = glm::perspective(glm::radians(60.f), 16.0f / 9.0f, 0.1f, 500.0f);
	ortho = glm::ortho(-640.0f, 640.0f, -360.0f, 360.0f, -500.0f, 500.0f);
}


void Camera::update()
{
	glm::mat4 cameraRotation = getRotationMatrix();

	position += glm::vec3(cameraRotation * glm::vec4(velocity, 0.f));
}

glm::mat4 Camera::getViewMatrix()
{
	// to create a correct model view, we need to move the world in opposite
	// direction to the camera
	//  so we will create the camera model matrix and invert
	glm::mat4 cameraTranslation = glm::translate(glm::mat4(1.f), position);
	glm::mat4 cameraRotation = getRotationMatrix();
	return glm::inverse(cameraTranslation * cameraRotation);

	glm::vec3 direction;
	direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
	direction.y = sin(glm::radians(pitch));
	direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
	camera_direction = direction;

	direction.x = -cos(glm::radians(yaw)) * sin(glm::radians(pitch));
	direction.y = cos(glm::radians(pitch));
	direction.z = -sin(glm::radians(yaw)) * sin(glm::radians(pitch));
	camera_up = direction;

	return glm::lookAt(position, position + camera_direction, camera_up);
}

glm::mat4 Camera::getRotationMatrix()
{
	// fairly typical FPS style camera. we join the pitch and yaw rotations into
	// the final rotation matrix
	glm::quat pitchRotation = glm::angleAxis(pitch, glm::vec3{ 1.f, 0.f, 0.f });
	glm::quat yawRotation = glm::angleAxis(yaw, glm::vec3{ 0.f, -1.f, 0.f });
	return glm::toMat4(yawRotation) * glm::toMat4(pitchRotation);
}

void Camera::processSDLEvent(SDL_Event& e)
{
	// 如果 ImGui 正在捕获鼠标事件，直接返回
	if (ImGui::GetIO().WantCaptureMouse)
		return;

	switch (e.type)
	{
	case SDL_MOUSEBUTTONDOWN:
		if (e.button.button == SDL_BUTTON_LEFT)
		{
			SDL_SetRelativeMouseMode(SDL_TRUE);
			SDL_ShowCursor(SDL_DISABLE);
			mouse_left_down = true;
		}
		break;
	case SDL_MOUSEBUTTONUP:
		if (e.button.button == SDL_BUTTON_LEFT)
		{
			SDL_SetRelativeMouseMode(SDL_FALSE);
			SDL_ShowCursor(SDL_ENABLE);
			mouse_left_down = false;
		}
		break;
	case SDL_KEYDOWN:
		if (e.key.keysym.sym == SDLK_w) { velocity.z = -1; }
		if (e.key.keysym.sym == SDLK_s) { velocity.z = 1; }
		if (e.key.keysym.sym == SDLK_a) { velocity.x = -1; }
		if (e.key.keysym.sym == SDLK_d) { velocity.x = 1; }
		if (e.key.keysym.sym == SDLK_LCTRL) { velocity.y = -1; }
		if (e.key.keysym.sym == SDLK_SPACE) { velocity.y = 1; }
		break;
	case SDL_KEYUP:
		if (e.key.keysym.sym == SDLK_w) { velocity.z = 0; }
		if (e.key.keysym.sym == SDLK_s) { velocity.z = 0; }
		if (e.key.keysym.sym == SDLK_a) { velocity.x = 0; }
		if (e.key.keysym.sym == SDLK_d) { velocity.x = 0; }
		if (e.key.keysym.sym == SDLK_LCTRL) { velocity.y = 0; }
		if (e.key.keysym.sym == SDLK_SPACE) { velocity.y = 0; }
		break;
	case SDL_MOUSEMOTION:
		if (mouse_left_down)
		{
			yaw += (float)e.motion.xrel / 200.f;
			pitch -= (float)e.motion.yrel / 200.f;
		}
		break;
	default:
		break;
	}
}

} // dk