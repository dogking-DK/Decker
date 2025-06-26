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
        // �����ļ���Datablock     ����: �ļ����ݵ����ݹ�ϣ
        virtual std::optional<AssetNode> import(const std::filesystem::path& file);
    };

#define REGISTER_IMPORTER(cls, name) \
    static bool _reg_##cls = []{ TypeRegistry::registerType(name, [](){return new (cls);}); return true; }();

}
