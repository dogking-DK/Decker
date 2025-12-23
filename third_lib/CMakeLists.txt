cmake_minimum_required(VERSION 3.14)
project(third_lib)

# Slang SDK 根目录（手动管理的第三方库）
set(SLANG_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/slang)

add_library(Slang STATIC IMPORTED)
set_target_properties(Slang PROPERTIES
    IMPORTED_LOCATION ${SLANG_ROOT}/lib/slang.lib
    INTERFACE_INCLUDE_DIRECTORIES ${SLANG_ROOT}/include
)

add_library(third_lib INTERFACE)
target_link_libraries(third_lib INTERFACE Slang)
target_include_directories(third_lib INTERFACE ${SLANG_ROOT}/include)
