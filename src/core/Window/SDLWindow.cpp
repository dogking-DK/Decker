/* Copyright (c) 2018-2023, Arm Limited and Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 the "License";
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "SDLWindow.h"

#include <SDL.h>

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

}

VkSurfaceKHR SDLWindow::create_surface(VkInstance instance, VkPhysicalDevice physical_device)
{

}

}
