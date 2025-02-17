#include "SDLWindow.h"

#include <SDL.h>
#include <SDL_vulkan.h>

namespace dk::core {
SDLWindow::SDLWindow(const Properties& properties) : Window(properties)
{
    SDL_LogSetAllPriority(SDL_LOG_PRIORITY_DEBUG);

    // We initialize SDL and create a window with it.
    SDL_Init(SDL_INIT_VIDEO);
    _window = SDL_CreateWindow(properties.title.c_str(),
                               SDL_WINDOWPOS_CENTERED,
                               SDL_WINDOWPOS_CENTERED,
                               static_cast<int>(properties.extent.width),
                               static_cast<int>(properties.extent.height),
                               SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if (!_window)
    {
        fmt::print("Failed to create SDL window");
    }
}

SDLWindow::~SDLWindow()
{
    SDL_DestroyWindow(_window);
}

VkSurfaceKHR SDLWindow::create_surface(vk::Instance& instance)
{
    VkSurfaceKHR surface;
    auto         sdl_err = SDL_Vulkan_CreateSurface(_window, instance, &surface);
    if (!sdl_err)
    {
        fmt::print(stderr, "Failed to create SDL\n");
    }
    return surface;
}

float SDLWindow::get_dpi_factor() const
{
    int   displayIndex = 0; // 默认显示器
    float ddpi, hdpi, vdpi;

    // 获取显示器的 DPI 信息
    if (SDL_GetDisplayDPI(displayIndex, &ddpi, &hdpi, &vdpi) == 0)
    {
        return ddpi;
    }
    return 1.0f;
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
}
