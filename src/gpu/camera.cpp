#define GLM_ENABLE_EXPERIMENTAL

#include <camera.h>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>

void Camera::update()
{
	glm::mat4 cameraRotation = getRotationMatrix();
	position += glm::vec3(cameraRotation * glm::vec4(velocity * 0.5f, 0.f));
}

void Camera::processSDLEvent(SDL_Event& e)
{
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

glm::mat4 Camera::getViewMatrix()
{
	// to create a correct model view, we need to move the world in opposite
	// direction to the camera
	//  so we will create the camera model matrix and invert
	glm::mat4 cameraTranslation = glm::translate(glm::mat4(1.f), position);
	glm::mat4 cameraRotation = getRotationMatrix();
	return glm::inverse(cameraTranslation * cameraRotation);
}

glm::mat4 Camera::getRotationMatrix()
{
	// fairly typical FPS style camera. we join the pitch and yaw rotations into
	// the final rotation matrix
	glm::quat pitchRotation = glm::angleAxis(pitch, glm::vec3{ 1.f, 0.f, 0.f });
	glm::quat yawRotation = glm::angleAxis(yaw, glm::vec3{ 0.f, -1.f, 0.f });
	return glm::toMat4(yawRotation) * glm::toMat4(pitchRotation);
}