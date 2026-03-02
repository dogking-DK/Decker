#include "PassRegistryJson.h"

#include <algorithm>
#include <fstream>
#include <vector>

#include <nlohmann/json.hpp>

namespace dk {
namespace {

const char* to_string(QueueClass queue)
{
    switch (queue)
    {
    case QueueClass::Graphics: return "Graphics";
    case QueueClass::Compute: return "Compute";
    case QueueClass::Transfer: return "Transfer";
    }
    return "Graphics";
}

const char* to_string(PinDirection direction)
{
    switch (direction)
    {
    case PinDirection::Input: return "Input";
    case PinDirection::Output: return "Output";
    }
    return "Input";
}

const char* to_string(ResourceKind kind)
{
    switch (kind)
    {
    case ResourceKind::Unknown: return "Unknown";
    case ResourceKind::Image: return "Image";
    case ResourceKind::Buffer: return "Buffer";
    }
    return "Unknown";
}

const char* to_string(ResourceUsage usage)
{
    switch (usage)
    {
    case ResourceUsage::Unknown: return "Unknown";
    case ResourceUsage::Sampled: return "Sampled";
    case ResourceUsage::StorageRead: return "StorageRead";
    case ResourceUsage::StorageWrite: return "StorageWrite";
    case ResourceUsage::ColorAttachment: return "ColorAttachment";
    case ResourceUsage::DepthStencilAttachment: return "DepthStencilAttachment";
    case ResourceUsage::TransferSrc: return "TransferSrc";
    case ResourceUsage::TransferDst: return "TransferDst";
    case ResourceUsage::Present: return "Present";
    case ResourceUsage::VertexBuffer: return "VertexBuffer";
    case ResourceUsage::IndexBuffer: return "IndexBuffer";
    case ResourceUsage::UniformBuffer: return "UniformBuffer";
    case ResourceUsage::IndirectBuffer: return "IndirectBuffer";
    }
    return "Unknown";
}

const char* to_string(ParamType type)
{
    switch (type)
    {
    case ParamType::Bool: return "Bool";
    case ParamType::Int: return "Int";
    case ParamType::Float: return "Float";
    case ParamType::Vec2: return "Vec2";
    case ParamType::Vec3: return "Vec3";
    case ParamType::Vec4: return "Vec4";
    case ParamType::String: return "String";
    }
    return "Float";
}

nlohmann::json build_pass_type_json(const PassTypeInfo& pass_info)
{
    nlohmann::json node{};
    node["type"] = pass_info.type;
    node["display_name"] = pass_info.displayName;
    node["queue"] = to_string(pass_info.queue);

    auto& pins = node["pins"] = nlohmann::json::array();
    for (const auto& pin : pass_info.pins)
    {
        pins.push_back(
            {
                {"name", pin.name},
                {"direction", to_string(pin.direction)},
                {"kind", to_string(pin.kind)},
                {"default_usage", to_string(pin.defaultUsage)},
                {"optional", pin.optional},
            });
    }

    auto& params = node["params"] = nlohmann::json::array();
    for (const auto& param : pass_info.params)
    {
        params.push_back(
            {
                {"name", param.name},
                {"type", to_string(param.type)},
                {"optional", param.optional},
            });
    }

    return node;
}

}

bool savePassRegistrySchemaJsonFile(const RenderPassRegistry& registry,
                                    const std::filesystem::path& output_path,
                                    std::string& out_error)
{
    std::vector<const PassTypeInfo*> pass_types = registry.all();
    std::ranges::sort(
        pass_types,
        [](const PassTypeInfo* lhs, const PassTypeInfo* rhs)
        {
            return lhs->type < rhs->type;
        });

    nlohmann::json schema{};
    schema["schema_version"] = 1;
    auto& types = schema["types"] = nlohmann::json::array();
    for (const auto* pass_info : pass_types)
    {
        if (!pass_info)
        {
            continue;
        }
        types.push_back(build_pass_type_json(*pass_info));
    }

    std::error_code ec;
    std::filesystem::create_directories(output_path.parent_path(), ec);
    if (ec)
    {
        out_error = "create_directories failed: " + ec.message();
        return false;
    }

    std::ofstream output(output_path, std::ios::binary | std::ios::trunc);
    if (!output.is_open())
    {
        out_error = "Failed to open output file: " + output_path.string();
        return false;
    }

    output << schema.dump(2);
    if (!output.good())
    {
        out_error = "Failed to write schema to file: " + output_path.string();
        return false;
    }

    return true;
}

}
