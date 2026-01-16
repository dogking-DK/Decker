cmake_minimum_required(VERSION 4.2)

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

find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(ASSET_DIR "${CMAKE_SOURCE_DIR}/assets")
set(GLSL_DIR "${ASSET_DIR}/shaders/glsl")
set(SPV_DIR "${ASSET_DIR}/shaders/spv")
set(SHADER_SCRIPT "${CMAKE_SOURCE_DIR}/assets/shaders/build_glsl_to_spv.py")

file(GLOB_RECURSE GLSL_FILES CONFIGURE_DEPENDS
    "${GLSL_DIR}/*.glsl"
    "${GLSL_DIR}/*.vert"
    "${GLSL_DIR}/*.frag"
    "${GLSL_DIR}/*.comp"
)

set(SHADER_STAMP "${CMAKE_BINARY_DIR}/shader_build.${CMAKE_CFG_INTDIR}.stamp")

add_custom_command(
    OUTPUT "${SHADER_STAMP}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SPV_DIR}"
    COMMAND ${Python3_EXECUTABLE} "${SHADER_SCRIPT}"
        --src "${GLSL_DIR}"
        --out "${SPV_DIR}"
        --config "$<IF:$<CONFIG:Debug>,debug,release>"
    COMMAND ${CMAKE_COMMAND} -E touch "${SHADER_STAMP}"
    DEPENDS ${GLSL_FILES} "${SHADER_SCRIPT}"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Compiling GLSL -> SPIR-V (assets)"
    VERBATIM
)

add_custom_target(compile_shaders ALL
    DEPENDS "${SHADER_STAMP}"
)

add_subdirectory(src)

add_dependencies(${PROJECT_NAME} compile_shaders)

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(CODE [[
if(CMAKE_INSTALL_CONFIG_NAME)
    set(_decker_config --config "${CMAKE_INSTALL_CONFIG_NAME}")
endif()
execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target compile_shaders ${_decker_config}
    RESULT_VARIABLE _decker_shader_result
)
if(NOT _decker_shader_result EQUAL 0)
    message(FATAL_ERROR "Shader compilation failed during install.")
endif()
]])

install(DIRECTORY "${CMAKE_SOURCE_DIR}/assets" DESTINATION bin)
