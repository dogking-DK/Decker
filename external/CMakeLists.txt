cmake_minimum_required(VERSION 3.14)
project(external)

# 将 vk-bootstrap 安装路径加入查找前缀，方便统一管理
set(CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/external/vk-bootstrap" ${CMAKE_PREFIX_PATH})

find_package(vk-bootstrap REQUIRED PATHS ${CMAKE_SOURCE_DIR}/external/vk-bootstrap NO_DEFAULT_PATH)
target_link_libraries(external vk-bootstrap::vk-bootstrap)
