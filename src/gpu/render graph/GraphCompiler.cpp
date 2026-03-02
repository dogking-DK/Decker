#include "GraphCompiler.h"

#include <queue>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include "PassRegistry.h"

namespace dk {
namespace {

bool validateUniqueNodeIds(const GraphAsset& graph, std::string& out_error)
{
    std::unordered_set<GraphNodeId> seen;
    seen.reserve(graph.nodes.size());
    for (const auto& node : graph.nodes)
    {
        if (!seen.insert(node.id).second)
        {
            out_error = "Duplicate node id: " + std::to_string(node.id);
            return false;
        }
    }
    return true;
}

bool validateUniqueResourceNames(const GraphAsset& graph, std::string& out_error)
{
    std::unordered_set<std::string> names;
    names.reserve(graph.resources.size());
    for (const auto& resource : graph.resources)
    {
        if (resource.name.empty())
        {
            out_error = "Resource has empty name";
            return false;
        }
        if (!names.insert(resource.name).second)
        {
            out_error = "Duplicate resource name: " + resource.name;
            return false;
        }
    }
    return true;
}

// 校验资源 kind 和 payload 变体一致，并做最基础 desc 合法性检查。
bool validateResourcePayloads(const GraphAsset& graph, std::string& out_error)
{
    for (const auto& resource : graph.resources)
    {
        if (resource.kind == ResourceKind::Image)
        {
            if (!std::holds_alternative<GraphImageDesc>(resource.payload))
            {
                out_error = "Resource \"" + resource.name + "\" kind is Image but payload is not GraphImageDesc";
                return false;
            }

            const auto& image = std::get<GraphImageDesc>(resource.payload);
            if (image.width == 0 || image.height == 0 || image.depth == 0)
            {
                out_error = "Resource \"" + resource.name + "\" has invalid image extent";
                return false;
            }
            continue;
        }

        if (resource.kind == ResourceKind::Buffer)
        {
            if (!std::holds_alternative<GraphBufferDesc>(resource.payload))
            {
                out_error = "Resource \"" + resource.name + "\" kind is Buffer but payload is not GraphBufferDesc";
                return false;
            }

            const auto& buffer = std::get<GraphBufferDesc>(resource.payload);
            if (resource.lifetime != ResourceLifetime::External && buffer.size == 0)
            {
                out_error = "Resource \"" + resource.name + "\" has zero buffer size";
                return false;
            }
            continue;
        }

        out_error = "Resource \"" + resource.name + "\" has unsupported kind";
        return false;
    }
    return true;
}

bool validateUniqueEdgeIds(const GraphAsset& graph, std::string& out_error)
{
    std::unordered_set<GraphEdgeId> seen;
    seen.reserve(graph.edges.size());
    for (const auto& edge : graph.edges)
    {
        if (!seen.insert(edge.id).second)
        {
            out_error = "Duplicate edge id: " + std::to_string(edge.id);
            return false;
        }
    }
    return true;
}

bool validateEdgeNodes(const GraphAsset& graph, std::string& out_error)
{
    std::unordered_set<GraphNodeId> node_ids;
    node_ids.reserve(graph.nodes.size());
    for (const auto& node : graph.nodes)
    {
        node_ids.insert(node.id);
    }

    for (const auto& edge : graph.edges)
    {
        if (!node_ids.contains(edge.fromNode))
        {
            out_error = "Edge " + std::to_string(edge.id) + " references missing fromNode " + std::to_string(edge.fromNode);
            return false;
        }
        if (!node_ids.contains(edge.toNode))
        {
            out_error = "Edge " + std::to_string(edge.id) + " references missing toNode " + std::to_string(edge.toNode);
            return false;
        }
    }
    return true;
}

bool validateUniqueInputPins(const GraphAsset& graph, std::string& out_error)
{
    std::unordered_map<GraphNodeId, std::unordered_set<std::string>> used_pins;
    used_pins.reserve(graph.nodes.size());

    for (const auto& edge : graph.edges)
    {
        if (edge.fromPin.empty())
        {
            out_error = "Edge " + std::to_string(edge.id) + " has empty from_pin";
            return false;
        }
        if (edge.toPin.empty())
        {
            out_error = "Edge " + std::to_string(edge.id) + " has empty to_pin";
            return false;
        }

        auto& pin_set = used_pins[edge.toNode];
        if (!pin_set.insert(edge.toPin).second)
        {
            out_error = "Node " + std::to_string(edge.toNode) +
                " has multiple incoming edges bound to pin \"" + edge.toPin + "\"";
            return false;
        }
    }
    return true;
}

bool validateRegisteredPassTypes(const GraphAsset& graph,
                                 const RenderPassRegistry& pass_registry,
                                 std::unordered_map<GraphNodeId, const PassTypeInfo*>& out_pass_info,
                                 std::string& out_error)
{
    out_pass_info.clear();
    out_pass_info.reserve(graph.nodes.size());

    for (const auto& node : graph.nodes)
    {
        const auto* pass_info = pass_registry.find(node.type);
        if (!pass_info)
        {
            out_error = "Node " + std::to_string(node.id) +
                " references unknown pass type \"" + node.type + "\"";
            return false;
        }
        out_pass_info.emplace(node.id, pass_info);
    }
    return true;
}

bool validateEdgePinsAndKinds(const GraphAsset& graph,
                              const std::unordered_map<GraphNodeId, const PassTypeInfo*>& pass_info_by_node,
                              std::string& out_error)
{
    for (const auto& edge : graph.edges)
    {
        const auto from_node_it = pass_info_by_node.find(edge.fromNode);
        const auto to_node_it = pass_info_by_node.find(edge.toNode);
        if (from_node_it == pass_info_by_node.end() || to_node_it == pass_info_by_node.end())
        {
            out_error = "Internal compile error: missing pass info for edge " + std::to_string(edge.id);
            return false;
        }

        const auto* from_pin = findPinSpec(*from_node_it->second, edge.fromPin);
        if (!from_pin)
        {
            out_error = "Edge " + std::to_string(edge.id) + " references missing output pin \"" + edge.fromPin +
                "\" on node " + std::to_string(edge.fromNode) + " (" + from_node_it->second->type + ")";
            return false;
        }
        if (from_pin->direction != PinDirection::Output)
        {
            out_error = "Edge " + std::to_string(edge.id) + " uses non-output pin \"" + edge.fromPin +
                "\" as source on node " + std::to_string(edge.fromNode);
            return false;
        }

        const auto* to_pin = findPinSpec(*to_node_it->second, edge.toPin);
        if (!to_pin)
        {
            out_error = "Edge " + std::to_string(edge.id) + " references missing input pin \"" + edge.toPin +
                "\" on node " + std::to_string(edge.toNode) + " (" + to_node_it->second->type + ")";
            return false;
        }
        if (to_pin->direction != PinDirection::Input)
        {
            out_error = "Edge " + std::to_string(edge.id) + " uses non-input pin \"" + edge.toPin +
                "\" as destination on node " + std::to_string(edge.toNode);
            return false;
        }

        if (from_pin->kind != ResourceKind::Unknown &&
            to_pin->kind != ResourceKind::Unknown &&
            from_pin->kind != to_pin->kind)
        {
            out_error = "Edge " + std::to_string(edge.id) + " kind mismatch: \"" + edge.fromPin + "\" (" +
                std::to_string(static_cast<std::uint32_t>(from_pin->kind)) + ") -> \"" + edge.toPin + "\" (" +
                std::to_string(static_cast<std::uint32_t>(to_pin->kind)) + ")";
            return false;
        }
    }
    return true;
}

// 仅接受字符串参数作为资源名覆盖输入（与 RenderSystem 的解析约定一致）。
bool hasStringParam(const GraphNodeAsset& node, std::string_view param_name)
{
    for (const auto& param : node.params)
    {
        if (param.name != param_name)
        {
            continue;
        }
        return std::holds_alternative<std::string>(param.value);
    }
    return false;
}

std::string pinAliasWithoutInputSuffix(const std::string& pin_name)
{
    constexpr std::string_view suffix = "_in";
    if (pin_name.size() <= suffix.size())
    {
        return pin_name;
    }

    const std::string_view name_view{pin_name};
    if (!name_view.ends_with(suffix))
    {
        return pin_name;
    }

    return pin_name.substr(0, pin_name.size() - suffix.size());
}

// 校验非 optional 输入 pin 至少有一条来源：edge 或字符串参数覆盖。
bool validateRequiredInputPins(const GraphAsset& graph,
                               const std::unordered_map<GraphNodeId, const PassTypeInfo*>& pass_info_by_node,
                               std::string& out_error)
{
    std::unordered_map<GraphNodeId, std::unordered_set<std::string>> incoming_input_pins;
    incoming_input_pins.reserve(graph.nodes.size());
    for (const auto& edge : graph.edges)
    {
        incoming_input_pins[edge.toNode].insert(edge.toPin);
    }

    for (const auto& node : graph.nodes)
    {
        const auto pass_it = pass_info_by_node.find(node.id);
        if (pass_it == pass_info_by_node.end())
        {
            out_error = "Internal compile error: missing pass info for node " + std::to_string(node.id);
            return false;
        }

        const auto inputs_it = incoming_input_pins.find(node.id);
        const auto* incoming = (inputs_it != incoming_input_pins.end()) ? &inputs_it->second : nullptr;

        for (const auto& pin : pass_it->second->pins)
        {
            if (pin.direction != PinDirection::Input || pin.optional)
            {
                continue;
            }

            const bool has_edge_binding = incoming ? incoming->contains(pin.name) : false;
            const std::string alias = pinAliasWithoutInputSuffix(pin.name);
            const bool has_param_binding = hasStringParam(node, pin.name) ||
                (alias != pin.name && hasStringParam(node, alias));

            if (!has_edge_binding && !has_param_binding)
            {
                out_error = "Node " + std::to_string(node.id) + " (" + node.type +
                    ") missing required input pin \"" + pin.name + "\"";
                return false;
            }
        }
    }

    return true;
}

}

bool compileGraphAsset(const GraphAsset& graph,
                       CompiledGraphAsset& out_compiled,
                       std::string& out_error,
                       const RenderPassRegistry* pass_registry)
{
    out_compiled = CompiledGraphAsset{};
    out_compiled.name = graph.name;
    out_compiled.schemaVersion = graph.schemaVersion;
    out_compiled.resources = graph.resources;

    if (!validateUniqueResourceNames(graph, out_error)) return false;
    if (!validateResourcePayloads(graph, out_error)) return false;
    if (!validateUniqueNodeIds(graph, out_error)) return false;
    if (!validateUniqueEdgeIds(graph, out_error)) return false;
    if (!validateEdgeNodes(graph, out_error)) return false;
    if (!validateUniqueInputPins(graph, out_error)) return false;
    if (pass_registry)
    {
        std::unordered_map<GraphNodeId, const PassTypeInfo*> pass_info_by_node;
        if (!validateRegisteredPassTypes(graph, *pass_registry, pass_info_by_node, out_error)) return false;
        if (!validateEdgePinsAndKinds(graph, pass_info_by_node, out_error)) return false;
        if (!validateRequiredInputPins(graph, pass_info_by_node, out_error)) return false;
    }

    const std::size_t node_count = graph.nodes.size();
    std::unordered_map<GraphNodeId, std::size_t> node_index;
    node_index.reserve(node_count);
    for (std::size_t i = 0; i < node_count; ++i)
    {
        node_index.emplace(graph.nodes[i].id, i);
    }

    std::vector<std::vector<std::size_t>> adjacency(node_count);
    std::vector<std::uint32_t>            indegree(node_count, 0);

    for (const auto& edge : graph.edges)
    {
        const auto from_it = node_index.find(edge.fromNode);
        const auto to_it   = node_index.find(edge.toNode);
        if (from_it == node_index.end() || to_it == node_index.end())
        {
            out_error = "Internal compile error: unresolved edge node";
            return false;
        }

        adjacency[from_it->second].push_back(to_it->second);
        indegree[to_it->second] += 1;
    }

    std::queue<std::size_t> zero_indegree;
    for (std::size_t i = 0; i < node_count; ++i)
    {
        if (indegree[i] == 0)
        {
            zero_indegree.push(i);
        }
    }

    out_compiled.executionOrder.reserve(node_count);
    while (!zero_indegree.empty())
    {
        const auto idx = zero_indegree.front();
        zero_indegree.pop();
        out_compiled.executionOrder.push_back(graph.nodes[idx].id);

        for (const auto dep_idx : adjacency[idx])
        {
            auto& d = indegree[dep_idx];
            d -= 1;
            if (d == 0)
            {
                zero_indegree.push(dep_idx);
            }
        }
    }

    if (out_compiled.executionOrder.size() != graph.nodes.size())
    {
        out_error = "Cycle detected in graph asset";
        return false;
    }

    std::unordered_map<GraphNodeId, std::unordered_map<std::string, CompiledPinSource>> input_bindings;
    input_bindings.reserve(node_count);
    for (const auto& edge : graph.edges)
    {
        input_bindings[edge.toNode].emplace(
            edge.toPin,
            CompiledPinSource{
                .sourceNode = edge.fromNode,
                .sourcePin = edge.fromPin
            });
    }

    out_compiled.nodes.reserve(node_count);
    for (const auto node_id : out_compiled.executionOrder)
    {
        const auto idx_it = node_index.find(node_id);
        if (idx_it == node_index.end())
        {
            out_error = "Internal compile error: node id not found in index";
            return false;
        }

        const auto& src_node = graph.nodes[idx_it->second];
        CompiledGraphNode compiled_node{};
        compiled_node.id = src_node.id;
        compiled_node.type = src_node.type;

        if (const auto bind_it = input_bindings.find(node_id); bind_it != input_bindings.end())
        {
            compiled_node.inputPins = bind_it->second;
        }

        out_compiled.nodes.push_back(std::move(compiled_node));
    }

    return true;
}

}
