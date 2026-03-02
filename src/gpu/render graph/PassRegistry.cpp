#include "PassRegistry.h"

namespace dk {

bool RenderPassRegistry::registerType(PassTypeInfo info)
{
    if (info.type.empty())
    {
        return false;
    }

    _types.insert_or_assign(info.type, std::move(info));
    return true;
}

const PassTypeInfo* RenderPassRegistry::find(std::string_view type) const
{
    const auto it = _types.find(std::string(type));
    if (it == _types.end())
    {
        return nullptr;
    }
    return &it->second;
}

std::vector<const PassTypeInfo*> RenderPassRegistry::all() const
{
    std::vector<const PassTypeInfo*> out;
    out.reserve(_types.size());
    for (const auto& [_, info] : _types)
    {
        out.push_back(&info);
    }
    return out;
}

}
