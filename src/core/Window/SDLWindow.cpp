#include "SDLWindow.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <SDL3/SDL_log.h>
#include "vk_debug_util.h"

namespace dk::core {
SDLWindow::SDLWindow(const Properties& properties) : Window(properties)
{
    SDL_SetLogPriorities(SDL_LOG_PRIORITY_DEBUG);
    // We initialize SDL and create a window with it.
    auto window_flags = SDL_WINDOW_VULKAN;
    if (properties.resizable)
    {
        window_flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;
    }
    SDL_Init(SDL_INIT_VIDEO);
    _window = SDL_CreateWindow(properties.title.c_str(),
                               static_cast<int>(properties.extent.width),
                               static_cast<int>(properties.extent.height),
                               window_flags);
    if (!_window)
    {
        fmt::print("Failed to create SDL window");
    }
}

SDLWindow::~SDLWindow()
{
    SDLWindow::close();
}

VkSurfaceKHR SDLWindow::create_surface(vk::Instance& instance)
{
    VkSurfaceKHR surface;
    auto         sdl_err = SDL_Vulkan_CreateSurface(_window, instance, nullptr, &surface);
    if (!sdl_err)
    {
        fmt::print(stderr, "Failed to create SDL\n");
    }
    return surface;
}

float SDLWindow::get_dpi_factor() const
{
    return SDL_GetWindowDisplayScale(_window);
}

float SDLWindow::get_content_scale_factor() const
{
    int scale_x, scale_y;
    int size_x,  size_y;

    // 获取窗口的缩放因子
    SDL_GetWindowSizeInPixels(_window, &scale_x, &scale_y);
    SDL_GetWindowSize(_window, &size_x, &size_y);

    return static_cast<float>(scale_x) / static_cast<float>(size_x);
}

void SDLWindow::close()
{
    SDL_DestroyWindow(_window);
}

bool SDLWindow::should_close()
{
    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        if (e.type == SDL_EVENT_QUIT)
        {
            return true;
        }
    }
    return false;
}

std::vector<std::string> SDLWindow::get_required_surface_extensions() const
{
    unsigned int             extensionCount;
    auto                     result = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
    std::vector<std::string> extensions(result, result + extensionCount);
    return extensions;
}
}
