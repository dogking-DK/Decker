#include "AssetDB.h"
#include <stdexcept>

namespace {
nlohmann::json deps_to_json(const dk::DependencyMap& deps)
{
    nlohmann::json j = nlohmann::json::object();
    for (auto& [k, v] : deps)
        j[dk::to_string(k)] = to_string(v);            // UUID → string
    return j;
}

std::optional<dk::AssetDependencyType> dependency_from_legacy_index(const std::size_t idx)
{
    // Historic array format stored only a single material dependency.
    if (idx == 0) return dk::AssetDependencyType::Material;
    return std::nullopt;
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
        "asset_type TEXT,"
        "importer TEXT,"
        "import_opts TEXT,"
        "deps TEXT,"
        "content_hash INTEGER,"
        "raw_path TEXT );";
    char* err = nullptr;
    if (sqlite3_exec(_db, sql, nullptr, nullptr, &err) != SQLITE_OK)
        throw std::runtime_error(err);

    // 兼容旧表，忽略重复列错误
    sqlite3_exec(_db, "ALTER TABLE asset_meta ADD COLUMN asset_type TEXT;", nullptr, nullptr, &err);
    if (err) sqlite3_free(err);
}

static void bindText(sqlite3_stmt* st, int idx, const std::string& s)
{
    sqlite3_bind_text(st, idx, s.c_str(), -1, SQLITE_TRANSIENT);
}

void AssetDB::upsert(const AssetMeta& m)
{
    auto sql =
        "REPLACE INTO asset_meta"
        "(uuid,asset_type,importer,import_opts,deps,content_hash,raw_path)"
        "VALUES(?,?,?,?,?,?,?);";
    sqlite3_stmt* st = nullptr;
    sqlite3_prepare_v2(_db, sql, -1, &st, nullptr);

    bindText(st, 1, as_string(m.uuid));
    bindText(st, 2, m.asset_type ? to_string(*m.asset_type) : "");
    bindText(st, 3, m.importer);
    bindText(st, 4, m.import_opts.dump());
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
    bindText(st, 5, deps_to_json(m.dependencies).dump());

    sqlite3_bind_int64(st, 6, static_cast<sqlite3_int64>(m.content_hash));
    bindText(st, 7, m.raw_path);

    sqlite3_step(st);
    sqlite3_finalize(st);
}

AssetMeta AssetDB::rowToMeta(sqlite3_stmt* st)
{
    AssetMeta m;
    const int column_count = sqlite3_column_count(st);

    // 0  uuid
    if (auto txt = reinterpret_cast<const char*>(sqlite3_column_text(st, 0)))
        m.uuid   = uuid_from_string(txt);

    // 1  asset_type (may be absent in legacy DB)
    if (column_count > 1)
    {
        if (auto txt = reinterpret_cast<const char*>(sqlite3_column_text(st, 1)))
        {
            if (auto type = asset_type_from_string(txt))
                m.asset_type = *type;
        }
    }

    // 2  importer
    if (column_count > 2)
    {
        if (auto txt   = reinterpret_cast<const char*>(sqlite3_column_text(st, 2)))
            m.importer = txt;
    }

    /* 2  import_opts  ──────────── */
    if (column_count > 3 && sqlite3_column_type(st, 3) == SQLITE_NULL)
        m.import_opts = nlohmann::json::object();          // {}
    else if (column_count > 3)
    {
        auto txt      = reinterpret_cast<const char*>(sqlite3_column_text(st, 3));
        m.import_opts = nlohmann::json::parse(txt, nullptr, /*throw*/false);
        if (m.import_opts.is_discarded())          // 解析失败
            m.import_opts = nlohmann::json::object();
    }

    /* 3  deps (uuid 数组/对象) ───────── */
    if (column_count > 4 && sqlite3_column_type(st, 4) != SQLITE_NULL)
    {
        auto txt = reinterpret_cast<const char*>(sqlite3_column_text(st, 4));
        auto j   = nlohmann::json::parse(txt, nullptr, false);

        /* 兼容旧版 array 格式 */
        if (j.is_array())
        {
            size_t idx = 0;
            for (auto& v : j)
            {
                auto id = UUID::from_string(v.get<std::string>());
                auto dep_type = dependency_from_legacy_index(idx++);
                if (id.has_value() && dep_type.has_value())
                    m.dependencies.emplace(dep_type.value(), id.value());
            }
        }
        /* 新版 object 格式 */
        else if (j.is_object())
        {
            for (auto& [k, v] : j.items())
            {
                auto id = UUID::from_string(v.get<std::string>());
                auto dep_type = dependency_from_string(k);
                if (id.has_value() && dep_type.has_value())
                    m.dependencies.emplace(dep_type.value(), id.value());
                //auto test = v.get<std::string>();
                //auto uid = uuid_from_string(test);
                //auto uid = uuid_from_string(test);
            }
        }
    }

    /* 4  content_hash */
    if (column_count > 5)
        m.content_hash = static_cast<uint64_t>(sqlite3_column_int64(st, 5));

    /* 5  raw_path */
    if (column_count > 6)
    {
        if (auto txt   = reinterpret_cast<const char*>(sqlite3_column_text(st, 6)))
            m.raw_path = txt;            // NULL → 留空串
    }

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
