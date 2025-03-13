#!/bin/bash

# 定义子模块列表
declare -A submodules=(
    ["vk-bootstrap"]="https://github.com/charles-lunarg/vk-bootstrap.git"
)

# 子模块根目录
submodule_root="external"

# 添加子模块
for name in "${!submodules[@]}"; do
    url="${submodules[$name]}"
    path="$submodule_root/$name"

    echo "Adding submodule: $name"
    echo "url: $url"
    git submodule add "$url" "$path"
    echo ""
done

# 初始化并更新所有子模块
echo "Initializing and updating submodules..."
git submodule update --init --recursive

echo press any button to continue...
read -n 1