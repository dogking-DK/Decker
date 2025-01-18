cmake_minimum_required(VERSION 3.11)

project(third_party LANGUAGES C CXX)

# set the use of C++23 globally as all examples require it
set(CMAKE_CXX_STANDARD 23)

# 禁用 Vulkan 查找，防止它使用默认的 vcpkg 安装
set(VCPKG_TARGET_TRIPLET "x64-windows" CACHE STRING "Vcpkg target triplet")
set(VCPKG_FEATURE_FLAGS "no-cmake-package" CACHE STRING "Vcpkg feature flags to disable automatic CMake package discovery")

# 禁用 imgui 和 volk 通过 vcpkg 查找
set(IMGUI_FIND_REQUIRED OFF)
set(VOLK_FIND_REQUIRED OFF)

find_package(Vulkan REQUIRED)

# ------ SDL3 ------

# 如果 SDL3 还没有通过 find_package 查找，可以手动配置
add_library(SDL3 INTERFACE)
target_include_directories(SDL3 INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(SDL3 INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/libSDL3.a)

# ------ imgui ------

set(IMGUI_SOURCES
    ./imgui/imgui.cpp
    ./imgui/imgui_draw.cpp
    ./imgui/imgui_tables.cpp
    ./imgui/imgui_widgets.cpp
    ./imgui/backends/imgui_impl_vulkan.cpp
    ./imgui/backends/imgui_impl_sdl3.cpp
)

add_library(imgui ${IMGUI_SOURCES})

target_include_directories(imgui PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/backends
)

# 添加 freetype 支持
option(IMGUI_ENABLE_FREETYPE "Enable FreeType support in ImGui" ON)
if (IMGUI_ENABLE_FREETYPE)
    target_compile_definitions(imgui PUBLIC IMGUI_ENABLE_FREETYPE)
    target_link_libraries(imgui PRIVATE freetype)
endif()

# Vulkan 和 SDL 支持
target_link_libraries(imgui PRIVATE Vulkan::Vulkan SDL3::SDL3)

# ------ implot ------

file(GLOB IMPLOT_SOURCES ./implot/*.cpp)

add_library(implot ${IMPLOT_SOURCES})

target_include_directories(implot PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(implot PUBLIC imgui)

# ------ volk ------

if (WIN32)
    set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR)
endif()

# 明确指定C语言
add_library(volk STATIC ./volk/volk.h)
target_include_directories(volk INTERFACE ./volk)
target_link_libraries(volk INTERFACE Vulkan::Vulkan)

# ------ vma ------

# vma 是一个头文件库，通常不需要源文件，直接用 INTERFACE 类型
add_library(vma INTERFACE)
target_include_directories(vma INTERFACE ./vma/include)
target_link_libraries(vma INTERFACE Vulkan::Vulkan)

# ------ vk-bootstrap ------

add_library(vk-bootstrap STATIC ./vk-bootstrap/src/VKBootstrap.cpp)
target_include_directories(vk-bootstrap PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(vk-bootstrap PRIVATE Vulkan::Vulkan)
