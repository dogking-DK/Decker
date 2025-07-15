#include "AssetDB.h"
#include <stdexcept>

namespace dk {
AssetDB& AssetDB::instance()
{
    static AssetDB d;
    return d;
}

void AssetDB::open(const std::filesystem::path& p)
{
    if (_db) return;
    if (sqlite3_open(p.string().c_str(), &_db) != SQLITE_OK)
        throw std::runtime_error("open db fail");
    prepareSchema();
}

void AssetDB::prepareSchema()
{
    auto sql =
        "CREATE TABLE IF NOT EXISTS asset_meta ("
        "uuid TEXT PRIMARY KEY,"
        "importer TEXT,"
        "import_opts TEXT,"
        "deps TEXT,"
        "content_hash INTEGER,"
        "raw_path TEXT );";
    char* err = nullptr;
    if (sqlite3_exec(_db, sql, nullptr, nullptr, &err) != SQLITE_OK)
        throw std::runtime_error(err);
}

static void bindText(sqlite3_stmt* st, int idx, const std::string& s)
{
    sqlite3_bind_text(st, idx, s.c_str(), -1, SQLITE_TRANSIENT);
}

void AssetDB::upsert(const AssetMeta& m)
{
    auto sql =
        "REPLACE INTO asset_meta"
        "(uuid,importer,import_opts,deps,content_hash,raw_path)"
        "VALUES(?,?,?,?,?,?);";
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(_db, sql, -1, &st, nullptr);

    bindText(st, 1, as_string(m.uuid));
    bindText(st, 2, m.importer);
    auto test = m.importOpts.dump();
    bindText(st, 3, m.importOpts.dump());
    std::string deps;
    for (size_t i = 0; i < m.dependencies.size(); ++i)
    {
        deps += as_string(m.dependencies[i]);
        if (i + 1 < m.dependencies.size()) deps += ",";
    }
    bindText(st, 4, deps);
    //bindText(st, 4, nlohmann::json(m.dependencies).dump());

    sqlite3_bind_int64(st, 5, static_cast<sqlite3_int64>(m.contentHash));
    bindText(st, 6, m.rawPath);

    sqlite3_step(st);
    sqlite3_finalize(st);
}

AssetMeta AssetDB::rowToMeta(sqlite3_stmt* st)
{
    AssetMeta m;

    // 0  uuid
    if (auto txt = reinterpret_cast<const char*>(sqlite3_column_text(st, 0)))
        m.uuid   = uuid_from_string(txt);

    // 1  importer
    if (auto txt   = reinterpret_cast<const char*>(sqlite3_column_text(st, 1)))
        m.importer = txt;

    /* 2  import_opts  ©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤ */
    if (sqlite3_column_type(st, 2) == SQLITE_NULL)
        m.importOpts = nlohmann::json::object();          // {}
    else
    {
        auto txt     = reinterpret_cast<const char*>(sqlite3_column_text(st, 2));
        m.importOpts = nlohmann::json::parse(txt, nullptr, /*throw*/false);
        if (m.importOpts.is_discarded())          // ½âÎöÊ§°Ü
            m.importOpts = nlohmann::json::object();
    }

    /* 3  deps (uuid Êý×é) ©¤©¤©¤©¤©¤©¤©¤©¤©¤ */
    if (sqlite3_column_type(st, 3) == SQLITE_NULL)
    {
        m.dependencies.clear();                   // []
    }
    else
    {
        auto txt  = reinterpret_cast<const char*>(sqlite3_column_text(st, 3));
        auto deps = nlohmann::json::parse(txt, nullptr, false);
        if (deps.is_array())
            for (auto& d : deps)
                m.dependencies.push_back(uuid_from_string(d.get<std::string>()));
    }

    /* 4  content_hash */
    m.contentHash = static_cast<uint64_t>(sqlite3_column_int64(st, 4));

    /* 5  raw_path */
    if (auto txt  = reinterpret_cast<const char*>(sqlite3_column_text(st, 5)))
        m.rawPath = txt;            // NULL ¡ú Áô¿Õ´®

    return m;
}


std::optional<AssetMeta> AssetDB::get(UUID id) const
{
    auto          sql = "SELECT * FROM asset_meta WHERE uuid=?;";
    sqlite3_stmt* st  = nullptr;
    sqlite3_prepare_v2(_db, sql, -1, &st, nullptr);
    bindText(st, 1, as_string(id));
    int rc = sqlite3_step(st);
    if (rc != SQLITE_ROW)
    {
        sqlite3_finalize(st);
        return std::nullopt;
    }
    auto meta = rowToMeta(st);
    sqlite3_finalize(st);
    return meta;
}

std::vector<AssetMeta> AssetDB::listAll() const
{
    std::vector<AssetMeta> v;
    auto                   sql = "SELECT * FROM asset_meta;";
    sqlite3_stmt*          st  = nullptr;
    sqlite3_prepare_v2(_db, sql, -1, &st, nullptr);
    while (sqlite3_step(st) == SQLITE_ROW) v.push_back(rowToMeta(st));
    sqlite3_finalize(st);
    return v;
}
}
