cmake_minimum_required(VERSION 3.10)

# 设置install目录
set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install")

# 设置vcpkg为tool chain
if(DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")

    message(STATUS "vcpkg is set as the toolchians")
else()
    message(FATAL_ERROR "Environment variable VCPKG_ROOT is not set!")
endif()

# 添加vsg库的目录
list(APPEND CMAKE_PREFIX_PATH "$ENV{VSG_PATH}/install/lib/cmake/vsg")
list(APPEND CMAKE_PREFIX_PATH "$ENV{VSGXCHANGE_PATH}/install/lib/cmake/vsgXchange")
list(APPEND CMAKE_PREFIX_PATH "$ENV{VSGIMGUI_PATH}/install/lib/cmake/vsgImGui")

project(Decker
        VERSION 0.0.1
        DESCRIPTION "a render based on vulkan"
        LANGUAGES CXX
)

# build all examples into the bin directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)

# Change the default build type to Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif(NOT CMAKE_BUILD_TYPE)

# find the vsg
if (VULKAN_SDK)
    set(ENV{VULKAN_SDK} ${VULKAN_SDK})
endif()

# 查找三方库
find_package(Vulkan)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(vk-bootstrap CONFIG REQUIRED)
find_package(assimp REQUIRED)
find_package(glslang REQUIRED)
find_package(flecs REQUIRED)
find_package(Freetype REQUIRED)
find_package(Tracy CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(volk CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(SDL2 CONFIG REQUIRED)
find_package(implot CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)

# 查找vsg的库
find_package(vsg REQUIRED)
find_package(vsgXchange REQUIRED)
find_package(vsgImGui REQUIRED)
# set the use of C++20 globally as all examples require it
set(CMAKE_CXX_STANDARD 20)

# add clobber build target to clear all the non git registered files/directories
add_custom_target(clobber
    COMMAND git clean -d -f -x
)

# 将资源文件从源目录复制到构建目录中的指定位置
file(COPY ${CMAKE_SOURCE_DIR}/assets DESTINATION ${CMAKE_BINARY_DIR})

# 查找所有源文件 (.cpp 和 .hpp)
file(GLOB_RECURSE MAIN_SOURCES "src/main/*.cpp" "src/main/*.hpp" "src/main/*.h") # 主入口
file(GLOB_RECURSE CORE_SOURCES "src/core/*.cpp" "src/core/*.hpp" "src/core/*.h") # 核心
file(GLOB_RECURSE GPU_SOURCES "src/gpu/*.cpp" "src/gpu/*.hpp" "src/gpu/*.h")     # GPU相关
file(GLOB_RECURSE IO_SOURCES "src/io/*.cpp" "src/io/*.hpp" "src/io/*.h")         # IO相关
file(GLOB_RECURSE UI_SOURCES "src/ui/*.cpp" "src/ui/*.hpp" "src/ui/*.h")         # UI相关

# 输出所有的源文件路径
message(STATUS "Main sources: ${MAIN_SOURCES}")
message(STATUS "Core sources: ${CORE_SOURCES}")
message(STATUS "GPU sources: ${GPU_SOURCES}")
message(STATUS "IO sources: ${IO_SOURCES}")
message(STATUS "UI sources: ${UI_SOURCES}")

# 将所有的源文件加入到一个列表
set(ALL_SOURCES
    ${MAIN_SOURCES}
    ${CORE_SOURCES}
    ${GPU_SOURCES}
    ${IO_SOURCES}
    ${UI_SOURCES}
)

# 设置source_group保持文件夹结构
source_group(TREE ${CMAKE_SOURCE_DIR}/src PREFIX "src" FILES ${ALL_SOURCES})

# 添加可执行文件
add_executable(${PROJECT_NAME} ${ALL_SOURCES})

# 链接三方库
target_link_libraries(${PROJECT_NAME} 
    Vulkan::Vulkan
    GPUOpen::VulkanMemoryAllocator
    volk::volk
    volk::volk_headers
    vsg::vsg 
    fmt::fmt
    vk-bootstrap::vk-bootstrap 
    vk-bootstrap::vk-bootstrap-compiler-warnings
    Tracy::TracyClient
    glm::glm-header-only
    Freetype::Freetype
    assimp::assimp
    $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
    $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
    spdlog::spdlog
    glslang::glslang
    glslang::glslang-default-resource-limits
    glslang::SPIRV
    glslang::SPVRemapper
    implot::implot
    imgui::imgui)

if (vsgXchange_FOUND)
    target_compile_definitions(${PROJECT_NAME} PRIVATE vsgXchange_FOUND)
    target_link_libraries(${PROJECT_NAME} vsgXchange::vsgXchange)
endif()

if (vsgImGui_FOUND)
    target_link_libraries(${PROJECT_NAME} vsgImGui::vsgImGui)
endif()

install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION bin
)

