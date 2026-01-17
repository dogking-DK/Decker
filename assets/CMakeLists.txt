# --- START OF FILE assets/CMakeLists.txt ---

# assets
find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(ASSET_DIR     "${CMAKE_SOURCE_DIR}/assets")
set(GLSL_DIR      "${ASSET_DIR}/shaders/glsl")
set(SPV_DIR       "${ASSET_DIR}/shaders/spv")
set(SHADER_SCRIPT "${ASSET_DIR}/shaders/build_glsl_to_spv.py")

# 1. 监控 glsl 目录下的所有 shader 文件
# CONFIGURE_DEPENDS 确保如果你新建了文件，CMake 会自动重新 Configure
file(GLOB_RECURSE GLSL_FILES CONFIGURE_DEPENDS
  "${GLSL_DIR}/*.glsl"
  "${GLSL_DIR}/*.vert"
  "${GLSL_DIR}/*.frag"
  "${GLSL_DIR}/*.comp"
  "${GLSL_DIR}/*.geom"
  "${GLSL_DIR}/*.tesc"
  "${GLSL_DIR}/*.tese"
  "${GLSL_DIR}/*.mesh"
  "${GLSL_DIR}/*.task"
  "${GLSL_DIR}/*.rgen"
  "${GLSL_DIR}/*.rmiss"
  "${GLSL_DIR}/*.rchit"
  "${GLSL_DIR}/*.rahit"
  "${GLSL_DIR}/*.rint"
  "${GLSL_DIR}/*.rcall"
)

# 2. 配置映射：将 VS 的 Debug/Release 转换为脚本需要的 debug/release 参数
set(SHADER_CFG "$<IF:$<CONFIG:Debug>,debug,release>")

# 3. 定义 Stamp 文件，用于标记编译状态
set(SHADER_STAMP "${CMAKE_BINARY_DIR}/shader_build.$<CONFIG>.stamp")

# 4. 定义编译命令
# 注意：我们将 SPV 输出到了源码目录的 assets/shaders/spv，这样后续复制 assets 文件夹时会带上它们
add_custom_command(
  OUTPUT  "${SHADER_STAMP}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${SPV_DIR}"
  COMMAND ${Python3_EXECUTABLE} "${SHADER_SCRIPT}"
          --src "${GLSL_DIR}"
          --out "${SPV_DIR}"
          --config "${SHADER_CFG}"
  COMMAND ${CMAKE_COMMAND} -E touch "${SHADER_STAMP}"
  DEPENDS ${GLSL_FILES} "${SHADER_SCRIPT}"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  COMMENT "Compiling GLSL -> SPV [${SHADER_CFG}]"
  VERBATIM
)

# 5. 定义 Target
# 注意：这里不加 ALL，而是让主程序去 depend 它，这样逻辑更清晰
add_custom_target(compile_shaders
  DEPENDS "${SHADER_STAMP}"
)