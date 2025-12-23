cmake_minimum_required(VERSION 3.11)

add_compile_definitions(VULKAN_NO_PROTOTYPE)

# ================== 查找第三方依赖 ==================
find_package(Vulkan REQUIRED)
find_package(VulkanHeaders CONFIG)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(VulkanUtilityLibraries CONFIG REQUIRED)
find_package(vk-bootstrap CONFIG REQUIRED)
find_package(Eigen3 CONFIG REQUIRED)
find_package(EnTT CONFIG REQUIRED)
find_package(Freetype REQUIRED)
find_package(Tracy CONFIG REQUIRED)
find_package(assimp REQUIRED)
find_package(fastgltf CONFIG REQUIRED)
find_package(flecs REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(glslang REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(implot CONFIG REQUIRED)
find_package(magic_enum CONFIG REQUIRED)
find_package(meshoptimizer CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(SDL3 CONFIG REQUIRED)
find_package(slang CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(Stb REQUIRED)
find_package(stduuid CONFIG REQUIRED)
find_package(tsl-robin-map CONFIG REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)

# ================== 收集源文件并保持分组结构 ==================
file(GLOB_RECURSE MAIN_SOURCES "main/*.cpp" "main/*.hpp" "main/*.h")
file(GLOB_RECURSE CORE_SOURCES "core/*.cpp" "core/*.hpp" "core/*.h")
file(GLOB_RECURSE ASSET_SOURCES "asset/*.cpp" "asset/*.hpp" "asset/*.h")
file(GLOB_RECURSE GPU_SOURCES "gpu/*.cpp" "gpu/*.hpp" "gpu/*.h")
file(GLOB_RECURSE IO_SOURCES "io/*.cpp" "io/*.hpp" "io/*.h")
file(GLOB_RECURSE UI_SOURCES "ui/*.cpp" "ui/*.hpp" "ui/*.h")
file(GLOB_RECURSE UTIL_SOURCES "util/*.cpp" "util/*.hpp" "util/*.h")
file(GLOB_RECURSE PHYSIC_SOURCES "physics/*.cpp" "physics/*.hpp" "physics/*.h")
file(GLOB_RECURSE MATH_SOURCES "math/*.cpp" "math/*.hpp" "math/*.h")
file(GLOB_RECURSE RUNTIME_SOURCES "runtime/*.cpp" "runtime/*.hpp" "runtime/*.h")
file(GLOB_RECURSE SCENE_SOURCES "scene/*.cpp" "scene/*.hpp" "scene/*.h")

message(STATUS "Main sources: ${MAIN_SOURCES}")
message(STATUS "Core sources: ${CORE_SOURCES}")
message(STATUS "asset sources: ${ASSET_SOURCES}")
message(STATUS "GPU sources: ${GPU_SOURCES}")
message(STATUS "IO sources: ${IO_SOURCES}")
message(STATUS "UI sources: ${UI_SOURCES}")
message(STATUS "Util sources: ${UTIL_SOURCES}")
message(STATUS "Scene sources: ${SCENE_SOURCES}")

set(ALL_SOURCES
    ${MAIN_SOURCES}
    ${CORE_SOURCES}
    ${ASSET_SOURCES}
    ${GPU_SOURCES}
    ${IO_SOURCES}
    ${UI_SOURCES}
    ${UTIL_SOURCES}
    ${PHYSIC_SOURCES}
    ${MATH_SOURCES}
    ${RUNTIME_SOURCES}
    ${SCENE_SOURCES}
)

source_group(TREE ${CMAKE_SOURCE_DIR}/src PREFIX "src" FILES ${ALL_SOURCES})

# ================== 生成可执行文件与包含路径 ==================
add_executable(${PROJECT_NAME} ${ALL_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src/main
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/asset
    ${CMAKE_SOURCE_DIR}/src/gpu
    ${CMAKE_SOURCE_DIR}/src/io
    ${CMAKE_SOURCE_DIR}/src/ui
    ${CMAKE_SOURCE_DIR}/src/util
    ${CMAKE_SOURCE_DIR}/src/physics
    ${CMAKE_SOURCE_DIR}/src/math
    ${CMAKE_SOURCE_DIR}/src/runtime
    ${CMAKE_SOURCE_DIR}/src/scene
    ${Stb_INCLUDE_DIR}
)

# ================== 链接第三方库 ==================
target_link_libraries(${PROJECT_NAME} PRIVATE
    Vulkan::Vulkan
    Vulkan::Headers
    Vulkan::SafeStruct
    Vulkan::LayerSettings
    Vulkan::UtilityHeaders
    Vulkan::CompilerConfiguration
    GPUOpen::VulkanMemoryAllocator
    vk-bootstrap::vk-bootstrap
    vk-bootstrap::vk-bootstrap-compiler-warnings
    Eigen3::Eigen
    EnTT::EnTT
    Freetype::Freetype
    Tracy::TracyClient
    assimp::assimp
    fastgltf::fastgltf
    fmt::fmt
    glslang::glslang
    glslang::glslang-default-resource-limits
    glslang::SPIRV
    glm::glm-header-only
    imgui::imgui
    implot::implot
    magic_enum::magic_enum
    meshoptimizer::meshoptimizer
    nlohmann_json::nlohmann_json
    SDL3::SDL3
    slang::slang
    spdlog::spdlog
    tsl::robin_map
    stduuid
    $<IF:$<TARGET_EXISTS:flecs::flecs>,flecs::flecs,flecs::flecs_static>
    unofficial::sqlite3::sqlite3
)

target_compile_definitions(${PROJECT_NAME} PRIVATE GLM_ENABLE_EXPERIMENTAL)

# ================== 资源目录同步到输出路径 ==================
if(UNIX AND NOT APPLE)
  add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E remove -f "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
    COMMAND ${CMAKE_COMMAND} -E create_symlink
            "${CMAKE_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
    COMMENT "Link assets -> $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets")
else()
  add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
    COMMENT "Copy assets -> $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets")
endif()
