#pragma once
#include <sqlite3.h>
#include <optional>

#include "AssetMeta.hpp"

namespace dk {
class AssetDB
{
public:
    static AssetDB& instance();

    void                     open(const std::filesystem::path& dbFile = "cache/assets.db");
    void                     upsert(const AssetMeta& m);
    std::optional<AssetMeta> get(UUID id) const;
    std::vector<AssetMeta>   listAll() const;

private:
    sqlite3*         _db = nullptr;
    void             prepareSchema();
    static AssetMeta rowToMeta(sqlite3_stmt*);
};
}
