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

bool parseResourceLifetime(const std::string& text, ResourceLifetime& out)
{
    if (text == "Transient") { out = ResourceLifetime::Transient; return true; }
    if (text == "External") { out = ResourceLifetime::External; return true; }
    if (text == "Persistent") { out = ResourceLifetime::Persistent; return true; }
    return false;
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

}

