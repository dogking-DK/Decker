#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

#include "AssetNode.hpp"

namespace dk{

    class Importer {
    public:
        virtual ~Importer() = default;
        // 解析文件→Datablock     返回: 文件内容的内容哈希
        virtual std::optional<AssetNode> import(const std::filesystem::path& file);
    };

#define REGISTER_IMPORTER(cls, name) \
    static bool _reg_##cls = []{ TypeRegistry::registerType(name, [](){return new (cls);}); return true; }();

}
