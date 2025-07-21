#include "AssetDB.h"
#include <stdexcept>

namespace {
    nlohmann::json deps_to_json(
        const tsl::robin_map<std::string, dk::UUID>& deps)
    {
        nlohmann::json j = nlohmann::json::object();
        for (auto& [k, v] : deps)
            j[k] = to_string(v);            // UUID → string
        return j;
    }
}

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
    bindText(st, 3, m.import_opts.dump());
    //std::string deps;
    //for (auto kv : m.dependencies)
    //{
    //    // 依赖是一个 map，key 是字符串，value 是 UUID
    //    deps += kv.first + ":" + as_string(kv.second);
    //    deps += ";"; // 分隔符
    //}
    //for (size_t i = 0; i < m.dependencies.size(); ++i)
    //{
    //    deps += as_string(m.dependencies.at(i));
    //    if (i + 1 < m.dependencies.size()) deps += ",";
    //}
    bindText(st, 4, deps_to_json(m.dependencies).dump());

    sqlite3_bind_int64(st, 5, static_cast<sqlite3_int64>(m.content_hash));
    bindText(st, 6, m.raw_path);

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

    /* 2  import_opts  ──────────── */
    if (sqlite3_column_type(st, 2) == SQLITE_NULL)
        m.import_opts = nlohmann::json::object();          // {}
    else
    {
        auto txt     = reinterpret_cast<const char*>(sqlite3_column_text(st, 2));
        m.import_opts = nlohmann::json::parse(txt, nullptr, /*throw*/false);
        if (m.import_opts.is_discarded())          // 解析失败
            m.import_opts = nlohmann::json::object();
    }

    /* 3  deps (uuid 数组) ───────── */
    if (sqlite3_column_type(st, 3) != SQLITE_NULL) 
    {
        const char* txt = reinterpret_cast<const char*>(sqlite3_column_text(st, 3));
        auto j = nlohmann::json::parse(txt, nullptr, false);

        /* 兼容旧版 array 格式 */
        if (j.is_array()) {
            size_t idx = 0;
            for (auto& v : j)
                m.dependencies.emplace(std::to_string(idx++), UUID::from_string(v.get<std::string>()));
        }
        /* 新版 object 格式 */
        else if (j.is_object()) {
            for (auto& [k, v] : j.items())
            {
                m.dependencies.emplace(k, UUID::from_string(v.get<std::string>()));
                //auto test = v.get<std::string>();
                //auto uid = uuid_from_string(test);
                //auto uid = uuid_from_string(test);
            }
        }
    }

    /* 4  content_hash */
    m.content_hash = static_cast<uint64_t>(sqlite3_column_int64(st, 4));

    /* 5  raw_path */
    if (auto txt  = reinterpret_cast<const char*>(sqlite3_column_text(st, 5)))
        m.raw_path = txt;            // NULL → 留空串

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
