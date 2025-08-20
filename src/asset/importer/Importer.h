﻿#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

#include "AssetMeta.hpp"
#include "AssetNode.hpp"

namespace dk {
struct ImportOptions
{
    bool                  only_nodes = true;                    // 仅节点树
    bool                  write_raw  = true;                   // 写 .raw*
    bool                  do_hash    = true;                    // 计算内容 hash
    std::filesystem::path raw_dir    = "cache/raw";
};

struct ImportResult
{
    std::vector<std::shared_ptr<AssetNode>> nodes;   // Hierarchy 用
    std::vector<AssetMeta>                  metas;   // Build / DB 用
};

class Importer
{
public:
    virtual                    ~Importer() = default;
    [[nodiscard]] virtual bool supportsExtension(const std::string& ext) const = 0;
    virtual ImportResult       import(const std::filesystem::path& file, const ImportOptions& opts = {}) = 0;
};

class ImporterRegistry
{
public:
    static ImporterRegistry& instance()
    {
        static ImporterRegistry r;
        return r;
    }

    void registerImporter(std::unique_ptr<Importer> impl)
    {
        importers.emplace_back(std::move(impl));
    }


    /// 根据文件扩展名选择第一个 supportsExtension() 返回 true 的 importer
    Importer* findImporter(const std::string& ext) const
    {
        for (auto& imp : importers)
        {
            if (imp->supportsExtension(ext))
                return imp.get();
        }
        return nullptr;
    }

    ImportResult import(const std::filesystem::path& file) const
    {
        // 找到第一个支持的 importer 并调用其 import 方法
        return findImporter(file.extension().string())->import(file);
    }

private:
    std::vector<std::unique_ptr<Importer>> importers;
};

/// 简化宏：在任意 .cpp 中添加一行，即可自动注册
#define REGISTER_IMPORTER(CLASS)                                 \
    namespace {                                                  \
      const bool _reg_##CLASS = [](){                            \
        ImporterRegistry::instance()                             \
            .registerImporter(std::make_unique<CLASS>());        \
        return true;                                             \
      }();                                                       \
    }
}
