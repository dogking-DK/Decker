#pragma once

#include <filesystem>
#include <string>

#include "PassRegistry.h"

namespace dk {

// 导出 pass schema 到 JSON，供节点编辑器/脚本工具读取。
bool savePassRegistrySchemaJsonFile(const RenderPassRegistry& registry,
                                    const std::filesystem::path& output_path,
                                    std::string& out_error);

}

