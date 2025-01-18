cmake_minimum_required(VERSION 3.11)

# 设置install目录
set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/install")
# set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/install" CACHE PATH "install directory" FORCE)

# 设置vcpkg为tool chain
if(DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")
    message(STATUS "vcpkg is set as the toolchians")
else()
    message(FATAL_ERROR "Environment variable VCPKG_ROOT is not set!")
endif()

project(Decker
        VERSION 0.0.1
        DESCRIPTION "a render based on vulkan"
        LANGUAGES CXX C
)

# build all examples into the bin directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)

# Change the default build type to Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif(NOT CMAKE_BUILD_TYPE)

if (VULKAN_SDK)
    set(ENV{VULKAN_SDK} ${VULKAN_SDK})
endif()

# set the use of C++23 globally as all examples require it
set(CMAKE_CXX_STANDARD 23)

# add clobber build target to clear all the non git registered files/directories
add_custom_target(clobber
    COMMAND git clean -d -f -x
)

# 将资源文件从源目录复制到构建目录中的指定位置
file(COPY ${CMAKE_SOURCE_DIR}/assets DESTINATION ${CMAKE_BINARY_DIR})

# add_subdirectory(external)

add_subdirectory(src)

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin         # 将可执行文件安装到 bin 目录
    LIBRARY DESTINATION lib         # 将共享库（如 DLL）安装到 lib 目录
    ARCHIVE DESTINATION lib         # 将静态库安装到 lib 目录
)