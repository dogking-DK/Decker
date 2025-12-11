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


# ===================== MSVC 全局编译/链接优化 =====================
if (MSVC)
    # 1. 所有配置都开启多核编译
    add_compile_options(/MP)

    # 2. 针对不同配置做定制
    # Debug：强调可调试，优化关，链接快
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Zi")
    set(CMAKE_C_FLAGS_DEBUG   "${CMAKE_C_FLAGS_DEBUG} /Zi")
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG    "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /DEBUG:FASTLINK")
    set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} /DEBUG:FASTLINK")

    # RelWithDebInfo：开优化+带调试符号，用于“跑起来 + 看栈”
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /Zi")
    set(CMAKE_C_FLAGS_RELWITHDEBINFO   "${CMAKE_C_FLAGS_RELWITHDEBINFO} /Zi")
    set(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO
        "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO} /DEBUG:FASTLINK")
    set(CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO
        "${CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO} /DEBUG:FASTLINK")

    # Release：只关心优化和体积，通常不需要 PDB（按需）
    # 如果你希望 Release 也带一点调试信息，可以类似加上 /Zi /DEBUG:FASTLINK
    # set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi")
    # set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /DEBUG:FASTLINK")
endif ()
# ===================== MSVC 优化结束 =====================

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
# add_subdirectory(third_lib)
add_subdirectory(src)

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin         # 将可执行文件安装到 bin 目录
    LIBRARY DESTINATION lib         # 将共享库（如 DLL）安装到 lib 目录
    ARCHIVE DESTINATION lib         # 将静态库安装到 lib 目录
)