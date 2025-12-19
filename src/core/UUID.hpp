#pragma once
#include <string_view>
#include <span>
#include <uuid.h>        // 来自 stduuid
#include <functional>
#include <imgui.h>
#include <fmt/base.h>
#include <random>

namespace dk {
using UUID = uuids::uuid;

inline UUID uuid_from_string(std::string_view str)
{
    uuids::uuid_name_generator gen(uuids::uuid::from_string("47183823-2574-4bfd-b411-99ed177d3e43").value());
    const uuids::uuid          id = gen(str);
    //fmt::print("UUID from string: {} {}\n", str, to_string(id));
    assert(!id.is_nil());
    assert(id.version() == uuids::uuid_version::name_based_sha1);
    assert(id.variant() == uuids::uuid_variant::rfc);
    return id;
}

inline UUID uuid_generate()
{
    static thread_local uuids::uuid_random_generator generator{std::random_device{}()};
    const auto id = generator();
    assert(!id.is_nil());
    assert(id.version() == uuids::uuid_version::random_number);
    assert(id.variant() == uuids::uuid_variant::rfc);
    return id;
}

inline UUID uuid_combine(const UUID& parent, uint64_t sub)
{
    const std::string buf = to_string(parent) + std::to_string(sub);
    return uuid_from_string(buf);                   // 再做一次 UUID5
}

inline std::string as_string(const UUID& id)
{
    return to_string(id);  // 使用 stduuid 的 to_string
}

/* ---------- 2️⃣  ImGui / STL 哈希 ---------- */

[[nodiscard]]
inline ImGuiID imgui_id(const UUID& id)
{
    return static_cast<ImGuiID>(std::hash<UUID>{}(id));   // 64→32 bit
}

// 简易反射注册表
class TypeRegistry
{
public:
    using Factory = void* (*)();
    static void  registerType(std::string_view name, Factory f);
    static void* create(std::string_view name);
};

} // namespace core
