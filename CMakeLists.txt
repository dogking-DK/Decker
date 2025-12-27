cmake_minimum_required(VERSION 3.11)

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

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMake Utility Targets")  # 这里改成你想要的文件夹名，比如 "Utility" 或 "CMake Targets"

# 将安装输出统一放到仓库下的 install 目录，便于清理与部署
set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/install")

# MSVC 平台的通用编译与链接优化
if (MSVC)
    add_compile_options(/MP)

    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Zi")
    set(CMAKE_C_FLAGS_DEBUG   "${CMAKE_C_FLAGS_DEBUG} /Zi")
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG    "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /DEBUG:FASTLINK")
    set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} /DEBUG:FASTLINK")

    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /Zi")
    set(CMAKE_C_FLAGS_RELWITHDEBINFO   "${CMAKE_C_FLAGS_RELWITHDEBINFO} /Zi")
    set(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO
        "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO} /DEBUG:FASTLINK")
    set(CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO
        "${CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO} /DEBUG:FASTLINK")
endif ()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif()

if (VULKAN_SDK)
    set(ENV{VULKAN_SDK} ${VULKAN_SDK})
endif()

set(CMAKE_CXX_STANDARD 23)

add_custom_target(clobber COMMAND git clean -d -f -x)

file(COPY ${CMAKE_SOURCE_DIR}/assets DESTINATION ${CMAKE_BINARY_DIR})

add_subdirectory(src)

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
