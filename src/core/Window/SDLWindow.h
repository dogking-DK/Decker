#pragma once
#include <SDL_video.h>

#include "Window.h"

namespace dk::core
{

	/**
	 * @brief An implementation of GLFW, inheriting the behaviour of the Window interface
	 */
	class SDLWindow : public Window
	{
	public:
		SDLWindow(const Window::Properties& properties);

		~SDLWindow() override;

		VkSurfaceKHR create_surface(vk::Instance& instance) override;

		VkSurfaceKHR create_surface(VkInstance instance, VkPhysicalDevice physical_device) override;

		bool should_close() override;

		void close() override;

		float get_dpi_factor() const override;

		float get_content_scale_factor() const override;

		std::vector<const char*> get_required_surface_extensions() const override;

	private:
		SDL_Window* _window{ nullptr };

	};
}        // namespace vkb
