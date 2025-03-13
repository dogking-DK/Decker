#include <Macros.h>
#include <vk_types.h>
#include <SDL3/SDL_events.h>
#include <Object.h>

namespace dk {

enum class VIEW_MODE
{
	perspective = 1,
	orthographic = 2,
};

class Camera
{
public:

	Camera();

	VIEW_MODE view_mode = VIEW_MODE::perspective;
	glm::vec3 velocity = glm::vec3(0.0f);
	glm::vec3 position = glm::vec3(0.0f);
	glm::vec3 camera_up = glm::vec3(0.0f);
	glm::vec3 camera_direction = glm::vec3(0.0f);
	glm::mat4 projection;
	glm::mat4 ortho;


	// vertical rotation
	float pitch{ 0.f };
	// horizontal rotation
	float yaw{ 0.f };

	glm::mat4 getViewMatrix();
	glm::mat4 getRotationMatrix();

	void processSDLEvent(SDL_Window* window, SDL_Event& e);


	bool mouse_left_down = false;

	void update();
};

} // dk