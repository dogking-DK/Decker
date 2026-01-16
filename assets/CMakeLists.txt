cmake_minimum_required(VERSION 3.11)

find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(ASSET_DIR     "${CMAKE_SOURCE_DIR}/assets")
set(GLSL_DIR      "${ASSET_DIR}/shaders/glsl")
set(SPV_DIR       "${ASSET_DIR}/shaders/spv")
set(SHADER_SCRIPT "${CMAKE_SOURCE_DIR}/assets/shaders/build_glsl_to_spv.py") # 按你实际路径改

# 收集 shader 源文件；新增/删除文件时自动触发 reconfigure
file(GLOB_RECURSE GLSL_FILES CONFIGURE_DEPENDS
  "${GLSL_DIR}/*.glsl"
  "${GLSL_DIR}/*.vert"
  "${GLSL_DIR}/*.frag"
  "${GLSL_DIR}/*.comp"
)

# 用 stamp 做 OUTPUT：任何 glsl 改动都会触发脚本（避免“总是运行”）
set(SHADER_STAMP "${CMAKE_BINARY_DIR}/shader_build.stamp")

add_custom_command(
  OUTPUT  "${SHADER_STAMP}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${SPV_DIR}"
  COMMAND ${Python3_EXECUTABLE} "${SHADER_SCRIPT}"
          --glsl_dir "${GLSL_DIR}"
          --spv_dir  "${SPV_DIR}"
  COMMAND ${CMAKE_COMMAND} -E touch "${SHADER_STAMP}"
  DEPENDS ${GLSL_FILES} "${SHADER_SCRIPT}"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  COMMENT "Compiling GLSL -> SPV (assets)"
  VERBATIM
)

add_custom_target(compile_shaders ALL
  DEPENDS "${SHADER_STAMP}"
)
set(ASSET_DST "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets") # 这个用到主 target 名称时会麻烦
