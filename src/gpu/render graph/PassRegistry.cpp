#include "PassRegistry.h"

#include <algorithm>
#include <cctype>

namespace dk {

namespace {
std::string normalize_type_name(std::string_view type)
{
    std::string normalized;
    normalized.reserve(type.size());

    for (const char ch : type)
    {
        if (ch == ' ' || ch == '_' || ch == '-' || ch == '\t')
        {
            continue;
        }
        normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }

    return normalized;
}
}

std::string RenderPassRegistry::normalizeTypeName(std::string_view type)
{
    return normalize_type_name(type);
}

bool RenderPassRegistry::registerType(PassTypeInfo info)
{
    const std::string key = normalize_type_name(info.type);
    if (key.empty())
    {
        return false;
    }

    if (info.displayName.empty())
    {
        info.displayName = info.type;
    }

    _types.insert_or_assign(key, std::move(info));
    return true;
}

const PassTypeInfo* RenderPassRegistry::find(std::string_view type) const
{
    const auto it = _types.find(normalize_type_name(type));
    if (it == _types.end())
    {
        return nullptr;
    }
    return &it->second;
}

const PinSpec* findPinSpec(const PassTypeInfo& pass_info, std::string_view pin_name)
{
    const auto pin_it = std::ranges::find_if(
        pass_info.pins,
        [pin_name](const PinSpec& pin)
        {
            return pin.name == pin_name;
        });

    if (pin_it == pass_info.pins.end())
    {
        return nullptr;
    }

    return &(*pin_it);
}

const ParamSpec* findParamSpec(const PassTypeInfo& pass_info, std::string_view param_name)
{
    const auto param_it = std::ranges::find_if(
        pass_info.params,
        [param_name](const ParamSpec& param)
        {
            return param.name == param_name;
        });

    if (param_it == pass_info.params.end())
    {
        return nullptr;
    }

    return &(*param_it);
}

const PinSpec* RenderPassRegistry::findPin(std::string_view type, std::string_view pin_name) const
{
    const auto* pass_info = find(type);
    if (!pass_info)
    {
        return nullptr;
    }
    return findPinSpec(*pass_info, pin_name);
}

const ParamSpec* RenderPassRegistry::findParam(std::string_view type, std::string_view param_name) const
{
    const auto* pass_info = find(type);
    if (!pass_info)
    {
        return nullptr;
    }
    return findParamSpec(*pass_info, param_name);
}

std::vector<const PassTypeInfo*> RenderPassRegistry::all() const
{
    std::vector<const PassTypeInfo*> out;
    out.reserve(_types.size());
    for (const auto& [key, info] : _types)
    {
        (void)key;
        out.push_back(&info);
    }
    return out;
}

}
