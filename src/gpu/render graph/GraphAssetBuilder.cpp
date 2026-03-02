#include "GraphAssetBuilder.h"

#include <utility>

#include "GraphCompiler.h"

namespace dk {

GraphAssetBuilder::GraphAssetBuilder(std::string name, std::uint32_t schema_version)
{
    _graph.schemaVersion = schema_version;
    _graph.name = std::move(name);
}

GraphResourceDesc& GraphAssetBuilder::addImageResource(std::string name,
                                                       const GraphImageDesc& desc,
                                                       ResourceLifetime lifetime,
                                                       std::string external_key)
{
    GraphResourceDesc resource{};
    resource.name = std::move(name);
    resource.kind = ResourceKind::Image;
    resource.lifetime = lifetime;
    resource.externalKey = std::move(external_key);
    resource.payload = desc;
    _graph.resources.push_back(std::move(resource));
    return _graph.resources.back();
}

GraphResourceDesc& GraphAssetBuilder::addBufferResource(std::string name,
                                                        const GraphBufferDesc& desc,
                                                        ResourceLifetime lifetime,
                                                        std::string external_key)
{
    GraphResourceDesc resource{};
    resource.name = std::move(name);
    resource.kind = ResourceKind::Buffer;
    resource.lifetime = lifetime;
    resource.externalKey = std::move(external_key);
    resource.payload = desc;
    _graph.resources.push_back(std::move(resource));
    return _graph.resources.back();
}

GraphNodeAsset& GraphAssetBuilder::addNode(std::string type,
                                           std::vector<GraphParam> params,
                                           GraphNodeId node_id)
{
    GraphNodeAsset node{};
    node.id = node_id == 0 ? _next_node_id++ : node_id;
    if (node.id >= _next_node_id)
    {
        _next_node_id = node.id + 1;
    }
    node.type = std::move(type);
    node.params = std::move(params);
    _graph.nodes.push_back(std::move(node));
    return _graph.nodes.back();
}

GraphEdgeAsset& GraphAssetBuilder::addEdge(GraphNodeId from_node,
                                           std::string from_pin,
                                           GraphNodeId to_node,
                                           std::string to_pin,
                                           GraphEdgeId edge_id)
{
    GraphEdgeAsset edge{};
    edge.id = edge_id == 0 ? _next_edge_id++ : edge_id;
    if (edge.id >= _next_edge_id)
    {
        _next_edge_id = edge.id + 1;
    }
    edge.fromNode = from_node;
    edge.fromPin = std::move(from_pin);
    edge.toNode = to_node;
    edge.toPin = std::move(to_pin);
    _graph.edges.push_back(std::move(edge));
    return _graph.edges.back();
}

GraphParam GraphAssetBuilder::makeBoolParam(std::string name, bool value) const
{
    return GraphParam{.name = std::move(name), .type = ParamType::Bool, .value = value};
}

GraphParam GraphAssetBuilder::makeIntParam(std::string name, std::int32_t value) const
{
    return GraphParam{.name = std::move(name), .type = ParamType::Int, .value = value};
}

GraphParam GraphAssetBuilder::makeFloatParam(std::string name, float value) const
{
    return GraphParam{.name = std::move(name), .type = ParamType::Float, .value = value};
}

GraphParam GraphAssetBuilder::makeVec2Param(std::string name, const std::array<float, 2>& value) const
{
    return GraphParam{.name = std::move(name), .type = ParamType::Vec2, .value = value};
}

GraphParam GraphAssetBuilder::makeVec3Param(std::string name, const std::array<float, 3>& value) const
{
    return GraphParam{.name = std::move(name), .type = ParamType::Vec3, .value = value};
}

GraphParam GraphAssetBuilder::makeVec4Param(std::string name, const std::array<float, 4>& value) const
{
    return GraphParam{.name = std::move(name), .type = ParamType::Vec4, .value = value};
}

GraphParam GraphAssetBuilder::makeStringParam(std::string name, std::string value) const
{
    return GraphParam{.name = std::move(name), .type = ParamType::String, .value = std::move(value)};
}

bool GraphAssetBuilder::validate(std::string& out_error, const RenderPassRegistry* pass_registry) const
{
    CompiledGraphAsset compiled{};
    return compileGraphAsset(_graph, compiled, out_error, pass_registry);
}

}

