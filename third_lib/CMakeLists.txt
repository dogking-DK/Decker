cmake_minimum_required(VERSION 3.14)
project(third_lib)

# 设置 Slang 目录
set(SLANG_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/slang)

# 设置 Slang 头文件
add_library(Slang STATIC IMPORTED)
set_target_properties(Slang PROPERTIES
    IMPORTED_LOCATION ${SLANG_ROOT}/lib/slang.lib  # Windows
    # IMPORTED_LOCATION ${SLANG_ROOT}/lib/libslang.a  # Linux/macOS（取消上面一行，启用这一行）
    INTERFACE_INCLUDE_DIRECTORIES ${SLANG_ROOT}/include
)

# 提供 Slang 目标给主项目使用
add_library(third_lib INTERFACE)
target_link_libraries(third_lib INTERFACE Slang)
target_include_directories(third_lib INTERFACE ${SLANG_ROOT}/include)

