cmake_minimum_required(VERSION 3.11)

find_package(PythonInterp REQUIRED)

set(ASSET_DIR "${CMAKE_SOURCE_DIR}/assets")
set(GLSL_DIR "${ASSET_DIR}/shaders/glsl")
set(SPV_DIR "${ASSET_DIR}/shaders/spv")
set(SHADER_SCRIPT "${CMAKE_SOURCE_DIR}/assets/shaders/build_glsl_to_spv.py")

if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.12")
  file(GLOB_RECURSE GLSL_FILES CONFIGURE_DEPENDS
    "${GLSL_DIR}/*.glsl"
    "${GLSL_DIR}/*.vert"
    "${GLSL_DIR}/*.frag"
    "${GLSL_DIR}/*.comp"
  )
else()
  file(GLOB_RECURSE GLSL_FILES
    "${GLSL_DIR}/*.glsl"
    "${GLSL_DIR}/*.vert"
    "${GLSL_DIR}/*.frag"
    "${GLSL_DIR}/*.comp"
  )
endif()

set(SHADER_STAMP "${CMAKE_BINARY_DIR}/shader_build.stamp")

add_custom_command(
  OUTPUT  "${SHADER_STAMP}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${SPV_DIR}"
  COMMAND ${PYTHON_EXECUTABLE} "${SHADER_SCRIPT}"
          --glsl_dir "${GLSL_DIR}"
          --spv_dir  "${SPV_DIR}"
  COMMAND ${CMAKE_COMMAND} -E touch "${SHADER_STAMP}"
  DEPENDS ${GLSL_FILES} "${SHADER_SCRIPT}"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  COMMENT "Compiling GLSL -> SPV"
  VERBATIM
)

add_custom_target(compile_shaders
  DEPENDS "${SHADER_STAMP}"
)

if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.12")
  file(GLOB_RECURSE ASSET_FILES CONFIGURE_DEPENDS
    "${ASSET_DIR}/*"
  )
else()
  file(GLOB_RECURSE ASSET_FILES
    "${ASSET_DIR}/*"
  )
endif()

set(ASSET_SYNC_STAMP "${CMAKE_BINARY_DIR}/asset_sync.stamp")
set(ASSET_TARGET "${PROJECT_NAME}")

add_custom_command(
  OUTPUT "${ASSET_SYNC_STAMP}"
  COMMAND ${CMAKE_COMMAND} -E remove_directory "$<TARGET_FILE_DIR:${ASSET_TARGET}>/assets"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${ASSET_DIR}"
          "$<TARGET_FILE_DIR:${ASSET_TARGET}>/assets"
  COMMAND ${CMAKE_COMMAND} -E touch "${ASSET_SYNC_STAMP}"
  DEPENDS ${ASSET_FILES} "${SHADER_STAMP}"
  COMMENT "Sync assets -> $<TARGET_FILE_DIR:${ASSET_TARGET}>/assets"
  VERBATIM
)

add_custom_target(sync_assets ALL
  DEPENDS "${ASSET_SYNC_STAMP}"
)
add_dependencies(sync_assets compile_shaders)

install(CODE
  "execute_process(COMMAND \"${PYTHON_EXECUTABLE}\" \"${SHADER_SCRIPT}\" --glsl_dir \"${GLSL_DIR}\" --spv_dir \"${SPV_DIR}\")"
)

install(DIRECTORY "${ASSET_DIR}"
  DESTINATION bin
  PATTERN "glsl" EXCLUDE
)
