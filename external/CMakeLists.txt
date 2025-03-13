set(CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/external/vk-bootstrap" ${CMAKE_PREFIX_PATH})

cmake_minimum_required(VERSION 3.14)
project(external)

# 设置 Slang 目录
set(SLANG_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/slang)

find_package(vk-bootstrap REQUIRED PATHS ${CMAKE_SOURCE_DIR}/external/vk-bootstrap NO_DEFAULT_PATH)
# find_package(vk-bootstrap REQUIRED)
target_link_libraries(external vk-bootstrap::vk-bootstrap)