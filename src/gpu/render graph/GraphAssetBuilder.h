#pragma once

#include <array>
#include <string>
#include <string_view>
#include <vector>

#include "GraphAsset.h"

namespace dk {
class RenderPassRegistry;

// 程序化构建 GraphAsset，便于后续 Python/C++ 统一使用同一套构图入口。
class GraphAssetBuilder
{
public:
    explicit GraphAssetBuilder(std::string name = "graph", std::uint32_t schema_version = 1);

    GraphResourceDesc& addImageResource(std::string name,
                                        const GraphImageDesc& desc,
                                        ResourceLifetime lifetime = ResourceLifetime::Transient,
                                        std::string external_key = {});
    GraphResourceDesc& addBufferResource(std::string name,
                                         const GraphBufferDesc& desc,
                                         ResourceLifetime lifetime = ResourceLifetime::Transient,
                                         std::string external_key = {});

    GraphNodeAsset& addNode(std::string type,
                            std::vector<GraphParam> params = {},
                            GraphNodeId node_id = 0);
    GraphEdgeAsset& addEdge(GraphNodeId from_node,
                            std::string from_pin,
                            GraphNodeId to_node,
                            std::string to_pin,
                            GraphEdgeId edge_id = 0);

    GraphParam makeBoolParam(std::string name, bool value) const;
    GraphParam makeIntParam(std::string name, std::int32_t value) const;
    GraphParam makeFloatParam(std::string name, float value) const;
    GraphParam makeVec2Param(std::string name, const std::array<float, 2>& value) const;
    GraphParam makeVec3Param(std::string name, const std::array<float, 3>& value) const;
    GraphParam makeVec4Param(std::string name, const std::array<float, 4>& value) const;
    GraphParam makeStringParam(std::string name, std::string value) const;

    const GraphAsset& asset() const { return _graph; }
    GraphAsset&& moveAsset() && { return std::move(_graph); }

    // 复用编译器校验，确保程序化生成的图和 JSON 图一致地被验证。
    bool validate(std::string& out_error, const RenderPassRegistry* pass_registry = nullptr) const;

private:
    GraphAsset _graph{};
    GraphNodeId _next_node_id{1};
    GraphEdgeId _next_edge_id{1};
};

}
