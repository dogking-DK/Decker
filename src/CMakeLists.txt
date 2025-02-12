cmake_minimum_required(VERSION 3.11)

add_compile_definitions(VULKAN_NO_PROTOTYPE)

# 查找三方库
find_package(Vulkan REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(vk-bootstrap CONFIG REQUIRED)
find_package(VulkanUtilityLibraries CONFIG REQUIRED)
find_package(VulkanHeaders CONFIG)
find_package(volk CONFIG REQUIRED)

find_package(assimp REQUIRED)
find_package(fastgltf CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(Stb REQUIRED)
find_package(glslang REQUIRED)
find_package(flecs REQUIRED)
find_package(magic_enum CONFIG REQUIRED)
find_package(Tracy CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(magic_enum REQUIRED)
# find_package(slang REQUIRED)

find_package(SDL2 CONFIG REQUIRED)
find_package(implot CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(Freetype REQUIRED)


# 查找所有源文件 (.cpp 和 .hpp)
file(GLOB_RECURSE MAIN_SOURCES "main/*.cpp" "main/*.hpp" "main/*.h") # 主入口
file(GLOB_RECURSE CORE_SOURCES "core/*.cpp" "core/*.hpp" "core/*.h") # 核心
file(GLOB_RECURSE GPU_SOURCES "gpu/*.cpp" "gpu/*.hpp" "gpu/*.h")     # GPU相关
file(GLOB_RECURSE IO_SOURCES "io/*.cpp" "io/*.hpp" "io/*.h")         # IO相关
file(GLOB_RECURSE UI_SOURCES "ui/*.cpp" "ui/*.hpp" "ui/*.h")         # UI相关
file(GLOB_RECURSE UTIL_SOURCES "util/*.cpp" "util/*.hpp" "util/*.h") # 工具类

# 输出所有的源文件路径
message(STATUS "Main sources: ${MAIN_SOURCES}")
message(STATUS "Core sources: ${CORE_SOURCES}")
message(STATUS "GPU sources: ${GPU_SOURCES}")
message(STATUS "IO sources: ${IO_SOURCES}")
message(STATUS "UI sources: ${UI_SOURCES}")
message(STATUS "Util sources: ${UTIL_SOURCES}")

# 将所有的源文件加入到一个列表
set(ALL_SOURCES
    ${MAIN_SOURCES}
    ${CORE_SOURCES}
    ${GPU_SOURCES}
    ${IO_SOURCES}
    ${UI_SOURCES}
    ${UTIL_SOURCES}
)

# 设置source_group保持文件夹结构
source_group(TREE ${CMAKE_SOURCE_DIR}/src PREFIX "src" FILES ${ALL_SOURCES})

# 添加可执行文件
add_executable(${PROJECT_NAME} ${ALL_SOURCES})

# 为目标添加 include 路径
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src/main
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/gpu
    ${CMAKE_SOURCE_DIR}/src/io
    ${CMAKE_SOURCE_DIR}/src/ui
    ${CMAKE_SOURCE_DIR}/src/util
    )

# 链接三方库
target_link_libraries(${PROJECT_NAME} 
    Vulkan::Vulkan
    Vulkan::Headers
    Vulkan::SafeStruct
    Vulkan::LayerSettings
    Vulkan::UtilityHeaders 
    Vulkan::CompilerConfiguration
    GPUOpen::VulkanMemoryAllocator
    volk::volk 
    volk::volk_headers
    vk-bootstrap::vk-bootstrap 
    vk-bootstrap::vk-bootstrap-compiler-warnings
    fmt::fmt
    Tracy::TracyClient
    glm::glm-header-only
    Freetype::Freetype
    freetype
    assimp::assimp
    fastgltf::fastgltf
    nlohmann_json::nlohmann_json
    $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
    $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
    spdlog::spdlog
    magic_enum::magic_enum
    glslang::glslang
    glslang::glslang-default-resource-limits
    glslang::SPIRV
    glslang::SPVRemapper
    # slang::slang
    implot::implot
    imgui::imgui

    third_lib
    )

# 链接 third_lib（间接链接 Slang）
# target_link_libraries(${PROJECT_NAME}  PRIVATE third_lib)

# target_link_libraries(${PROJECT_NAME} 
#     vma
#     volk
#     vk-bootstrap
#     imgui
#     implot
# )

# 链接三方头文件
target_include_directories(${PROJECT_NAME} PRIVATE ${Stb_INCLUDE_DIR})