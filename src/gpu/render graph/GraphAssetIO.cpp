#include "GraphAssetIO.h"

#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

namespace dk {
namespace {
using json = nlohmann::json;

bool parseResourceKind(const std::string& text, ResourceKind& out)
{
    if (text == "Image") { out = ResourceKind::Image; return true; }
    if (text == "Buffer") { out = ResourceKind::Buffer; return true; }
    return false;
}

const char* toResourceKindText(ResourceKind kind)
{
    switch (kind)
    {
    case ResourceKind::Image: return "Image";
    case ResourceKind::Buffer: return "Buffer";
    case ResourceKind::Unknown: return "Unknown";
    }
    return "Unknown";
}

bool parseResourceLifetime(const std::string& text, ResourceLifetime& out)
{
    if (text == "Transient") { out = ResourceLifetime::Transient; return true; }
    if (text == "External") { out = ResourceLifetime::External; return true; }
    if (text == "Persistent") { out = ResourceLifetime::Persistent; return true; }
    return false;
}

const char* toResourceLifetimeText(ResourceLifetime lifetime)
{
    switch (lifetime)
    {
    case ResourceLifetime::Transient: return "Transient";
    case ResourceLifetime::External: return "External";
    case ResourceLifetime::Persistent: return "Persistent";
    }
    return "Transient";
}

bool parseParamType(const std::string& text, ParamType& out)
{
    if (text == "Bool") { out = ParamType::Bool; return true; }
    if (text == "Int") { out = ParamType::Int; return true; }
    if (text == "Float") { out = ParamType::Float; return true; }
    if (text == "Vec2") { out = ParamType::Vec2; return true; }
    if (text == "Vec3") { out = ParamType::Vec3; return true; }
    if (text == "Vec4") { out = ParamType::Vec4; return true; }
    if (text == "String") { out = ParamType::String; return true; }
    return false;
}

const char* toParamTypeText(ParamType type)
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

template <typename T>
bool readOptional(const json& object, const char* key, T& out)
{
    auto it = object.find(key);
    if (it == object.end()) return false;
    out = it->get<T>();
    return true;
}

std::string toString(const std::filesystem::path& path)
{
    return path.string();
}

bool parseParamValue(const json& value, ParamType type, ParamValue& out, std::string& out_error)
{
    try
    {
        switch (type)
        {
        case ParamType::Bool:
            out = value.get<bool>();
            return true;
        case ParamType::Int:
            out = value.get<std::int32_t>();
            return true;
        case ParamType::Float:
            out = value.get<float>();
            return true;
        case ParamType::String:
            out = value.get<std::string>();
            return true;
        case ParamType::Vec2:
        {
            if (!value.is_array() || value.size() != 2)
            {
                out_error = "Vec2 parameter expects array of 2 floats";
                return false;
            }
            out = std::array<float, 2>{value[0].get<float>(), value[1].get<float>()};
            return true;
        }
        case ParamType::Vec3:
        {
            if (!value.is_array() || value.size() != 3)
            {
                out_error = "Vec3 parameter expects array of 3 floats";
                return false;
            }
            out = std::array<float, 3>{value[0].get<float>(), value[1].get<float>(), value[2].get<float>()};
            return true;
        }
        case ParamType::Vec4:
        {
            if (!value.is_array() || value.size() != 4)
            {
                out_error = "Vec4 parameter expects array of 4 floats";
                return false;
            }
            out = std::array<float, 4>{value[0].get<float>(), value[1].get<float>(), value[2].get<float>(), value[3].get<float>()};
            return true;
        }
        }
    }
    catch (const std::exception& e)
    {
        out_error = e.what();
        return false;
    }

    out_error = "Unsupported parameter type";
    return false;
}

bool parseResources(const json& root, GraphAsset& out_graph, std::string& out_error)
{
    auto res_it = root.find("resources");
    if (res_it == root.end()) return true;
    if (!res_it->is_array())
    {
        out_error = "\"resources\" must be an array";
        return false;
    }

    out_graph.resources.reserve(res_it->size());
    for (const auto& res_json : *res_it)
    {
        GraphResourceDesc desc{};

        desc.name = res_json.value("name", "");
        if (desc.name.empty())
        {
            out_error = "Resource requires non-empty \"name\"";
            return false;
        }

        const std::string kind_text = res_json.value("kind", "Unknown");
        if (!parseResourceKind(kind_text, desc.kind))
        {
            out_error = "Resource \"" + desc.name + "\" has invalid kind: " + kind_text;
            return false;
        }

        const std::string life_text = res_json.value("lifetime", "Transient");
        if (!parseResourceLifetime(life_text, desc.lifetime))
        {
            out_error = "Resource \"" + desc.name + "\" has invalid lifetime: " + life_text;
            return false;
        }

        desc.externalKey = res_json.value("external_key", res_json.value("externalKey", std::string{}));

        const auto payload_it = res_json.find("desc");
        if (payload_it == res_json.end() || !payload_it->is_object())
        {
            out_error = "Resource \"" + desc.name + "\" requires object field \"desc\"";
            return false;
        }

        const auto& payload = *payload_it;
        if (desc.kind == ResourceKind::Image)
        {
            GraphImageDesc image{};
            readOptional(payload, "width", image.width);
            readOptional(payload, "height", image.height);
            readOptional(payload, "depth", image.depth);
            readOptional(payload, "mip_levels", image.mipLevels);
            readOptional(payload, "mipLevels", image.mipLevels);
            readOptional(payload, "array_layers", image.arrayLayers);
            readOptional(payload, "arrayLayers", image.arrayLayers);
            readOptional(payload, "format", image.format);
            readOptional(payload, "usage", image.usage);
            readOptional(payload, "samples", image.samples);
            readOptional(payload, "aspect_mask", image.aspectMask);
            readOptional(payload, "aspectMask", image.aspectMask);
            desc.payload = image;
        }
        else
        {
            GraphBufferDesc buffer{};
            readOptional(payload, "size", buffer.size);
            readOptional(payload, "usage", buffer.usage);
            desc.payload = buffer;
        }

        out_graph.resources.push_back(std::move(desc));
    }

    return true;
}

bool parseNodes(const json& root, GraphAsset& out_graph, std::string& out_error)
{
    auto nodes_it = root.find("nodes");
    if (nodes_it == root.end()) return true;
    if (!nodes_it->is_array())
    {
        out_error = "\"nodes\" must be an array";
        return false;
    }

    out_graph.nodes.reserve(nodes_it->size());
    for (const auto& node_json : *nodes_it)
    {
        GraphNodeAsset node{};
        node.id = node_json.value("id", 0u);
        node.type = node_json.value("type", "");
        if (node.id == 0 || node.type.empty())
        {
            out_error = "Node requires non-zero \"id\" and non-empty \"type\"";
            return false;
        }

        auto params_it = node_json.find("params");
        if (params_it != node_json.end())
        {
            if (!params_it->is_array())
            {
                out_error = "Node \"params\" must be an array";
                return false;
            }

            node.params.reserve(params_it->size());
            for (const auto& param_json : *params_it)
            {
                GraphParam param{};
                param.name = param_json.value("name", "");
                if (param.name.empty())
                {
                    out_error = "Node param requires non-empty \"name\"";
                    return false;
                }

                const std::string type_text = param_json.value("type", "Float");
                if (!parseParamType(type_text, param.type))
                {
                    out_error = "Node param \"" + param.name + "\" has invalid type: " + type_text;
                    return false;
                }

                auto value_it = param_json.find("value");
                if (value_it == param_json.end())
                {
                    out_error = "Node param \"" + param.name + "\" requires \"value\"";
                    return false;
                }

                std::string parse_error;
                if (!parseParamValue(*value_it, param.type, param.value, parse_error))
                {
                    out_error = "Node param \"" + param.name + "\" parse failed: " + parse_error;
                    return false;
                }

                node.params.push_back(std::move(param));
            }
        }

        out_graph.nodes.push_back(std::move(node));
    }

    return true;
}

bool parseEdges(const json& root, GraphAsset& out_graph, std::string& out_error)
{
    auto edges_it = root.find("edges");
    if (edges_it == root.end()) return true;
    if (!edges_it->is_array())
    {
        out_error = "\"edges\" must be an array";
        return false;
    }

    out_graph.edges.reserve(edges_it->size());
    for (const auto& edge_json : *edges_it)
    {
        GraphEdgeAsset edge{};
        edge.id = edge_json.value("id", 0u);
        edge.fromNode = edge_json.value("from_node", edge_json.value("fromNode", 0u));
        edge.toNode = edge_json.value("to_node", edge_json.value("toNode", 0u));
        edge.fromPin = edge_json.value("from_pin", edge_json.value("fromPin", std::string{}));
        edge.toPin = edge_json.value("to_pin", edge_json.value("toPin", std::string{}));

        if (edge.id == 0 || edge.fromNode == 0 || edge.toNode == 0 || edge.fromPin.empty() || edge.toPin.empty())
        {
            out_error = "Edge requires id/from/to node ids and from/to pin names";
            return false;
        }

        out_graph.edges.push_back(std::move(edge));
    }

    return true;
}

bool parseGraphJson(const json& root, GraphAsset& out_graph, std::string& out_error)
{
    out_graph = GraphAsset{};
    out_graph.schemaVersion = root.value("schema_version", root.value("schemaVersion", 1u));
    out_graph.name = root.value("name", std::string{"graph"});

    if (!parseResources(root, out_graph, out_error)) return false;
    if (!parseNodes(root, out_graph, out_error)) return false;
    if (!parseEdges(root, out_graph, out_error)) return false;
    return true;
}

json paramValueToJson(const GraphParam& param)
{
    switch (param.type)
    {
    case ParamType::Bool:
        return std::get<bool>(param.value);
    case ParamType::Int:
        return std::get<std::int32_t>(param.value);
    case ParamType::Float:
        return std::get<float>(param.value);
    case ParamType::String:
        return std::get<std::string>(param.value);
    case ParamType::Vec2:
    {
        const auto value = std::get<std::array<float, 2>>(param.value);
        return json::array({value[0], value[1]});
    }
    case ParamType::Vec3:
    {
        const auto value = std::get<std::array<float, 3>>(param.value);
        return json::array({value[0], value[1], value[2]});
    }
    case ParamType::Vec4:
    {
        const auto value = std::get<std::array<float, 4>>(param.value);
        return json::array({value[0], value[1], value[2], value[3]});
    }
    }
    return nullptr;
}

bool writeGraphJson(const GraphAsset& graph, json& out_root, std::string& out_error)
{
    out_root = json::object();
    out_root["schema_version"] = graph.schemaVersion;
    out_root["name"] = graph.name;

    auto& resources_json = out_root["resources"] = json::array();
    for (const auto& resource : graph.resources)
    {
        json resource_json = json::object();
        resource_json["name"] = resource.name;
        resource_json["kind"] = toResourceKindText(resource.kind);
        resource_json["lifetime"] = toResourceLifetimeText(resource.lifetime);
        if (!resource.externalKey.empty())
        {
            resource_json["external_key"] = resource.externalKey;
        }

        json desc_json = json::object();
        if (resource.kind == ResourceKind::Image)
        {
            if (!std::holds_alternative<GraphImageDesc>(resource.payload))
            {
                out_error = "Resource \"" + resource.name + "\" kind/payload mismatch while saving";
                return false;
            }
            const auto& image = std::get<GraphImageDesc>(resource.payload);
            desc_json["width"] = image.width;
            desc_json["height"] = image.height;
            desc_json["depth"] = image.depth;
            desc_json["mip_levels"] = image.mipLevels;
            desc_json["array_layers"] = image.arrayLayers;
            desc_json["format"] = image.format;
            desc_json["usage"] = image.usage;
            desc_json["samples"] = image.samples;
            desc_json["aspect_mask"] = image.aspectMask;
        }
        else if (resource.kind == ResourceKind::Buffer)
        {
            if (!std::holds_alternative<GraphBufferDesc>(resource.payload))
            {
                out_error = "Resource \"" + resource.name + "\" kind/payload mismatch while saving";
                return false;
            }
            const auto& buffer = std::get<GraphBufferDesc>(resource.payload);
            desc_json["size"] = buffer.size;
            desc_json["usage"] = buffer.usage;
        }
        else
        {
            out_error = "Resource \"" + resource.name + "\" has unsupported kind while saving";
            return false;
        }
        resource_json["desc"] = std::move(desc_json);
        resources_json.push_back(std::move(resource_json));
    }

    auto& nodes_json = out_root["nodes"] = json::array();
    for (const auto& node : graph.nodes)
    {
        json node_json = json::object();
        node_json["id"] = node.id;
        node_json["type"] = node.type;

        auto& params_json = node_json["params"] = json::array();
        for (const auto& param : node.params)
        {
            params_json.push_back(
                {
                    {"name", param.name},
                    {"type", toParamTypeText(param.type)},
                    {"value", paramValueToJson(param)},
                });
        }

        nodes_json.push_back(std::move(node_json));
    }

    auto& edges_json = out_root["edges"] = json::array();
    for (const auto& edge : graph.edges)
    {
        edges_json.push_back(
            {
                {"id", edge.id},
                {"from_node", edge.fromNode},
                {"from_pin", edge.fromPin},
                {"to_node", edge.toNode},
                {"to_pin", edge.toPin},
            });
    }

    return true;
}
}

bool loadGraphAssetFromJsonFile(const std::filesystem::path& path, GraphAsset& out_graph, std::string& out_error)
{
    std::ifstream in(path);
    if (!in.is_open())
    {
        out_error = "Failed to open graph asset file: " + toString(path);
        return false;
    }

    std::stringstream ss;
    ss << in.rdbuf();
    return loadGraphAssetFromJsonText(ss.str(), out_graph, out_error);
}

bool loadGraphAssetFromJsonText(const std::string& json_text, GraphAsset& out_graph, std::string& out_error)
{
    try
    {
        const auto root = nlohmann::json::parse(json_text);
        return parseGraphJson(root, out_graph, out_error);
    }
    catch (const std::exception& e)
    {
        out_error = e.what();
        return false;
    }
}

bool saveGraphAssetToJsonFile(const std::filesystem::path& path, const GraphAsset& graph, std::string& out_error)
{
    std::string json_text;
    if (!saveGraphAssetToJsonText(graph, json_text, out_error))
    {
        return false;
    }

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out.is_open())
    {
        out_error = "Failed to open graph asset file for write: " + toString(path);
        return false;
    }

    out << json_text;
    if (!out.good())
    {
        out_error = "Failed to write graph asset file: " + toString(path);
        return false;
    }

    return true;
}

bool saveGraphAssetToJsonText(const GraphAsset& graph, std::string& out_json_text, std::string& out_error)
{
    try
    {
        json root = json::object();
        if (!writeGraphJson(graph, root, out_error))
        {
            return false;
        }
        out_json_text = root.dump(2);
        return true;
    }
    catch (const std::exception& e)
    {
        out_error = e.what();
        return false;
    }
}

}
