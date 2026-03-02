#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "GraphAsset.h"

namespace dk {

enum class PinDirection : std::uint8_t
{
    Input,
    Output,
};

struct PinSpec
{
    std::string   name;
    PinDirection  direction{PinDirection::Input};
    ResourceKind  kind{ResourceKind::Unknown};
    ResourceUsage defaultUsage{ResourceUsage::Unknown};
    bool          optional{false};
};

struct ParamSpec
{
    std::string name;
    ParamType   type{ParamType::Float};
    bool        optional{false};
};

struct PassTypeInfo
{
    std::string           type;
    std::string           displayName;
    QueueClass            queue{QueueClass::Graphics};
    std::vector<PinSpec>  pins;
    std::vector<ParamSpec> params;
};

const PinSpec* findPinSpec(const PassTypeInfo& pass_info, std::string_view pin_name);
const ParamSpec* findParamSpec(const PassTypeInfo& pass_info, std::string_view param_name);

class RenderPassRegistry
{
public:
    static std::string normalizeTypeName(std::string_view type);

    bool registerType(PassTypeInfo info);
    const PassTypeInfo* find(std::string_view type) const;
    const PinSpec* findPin(std::string_view type, std::string_view pin_name) const;
    const ParamSpec* findParam(std::string_view type, std::string_view param_name) const;
    std::vector<const PassTypeInfo*> all() const;

private:
    std::unordered_map<std::string, PassTypeInfo> _types;
};

}
