#include "GraphCompiler.h"

#include <queue>
#include <unordered_map>
#include <unordered_set>

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

}

bool compileGraphAsset(const GraphAsset& graph, CompiledGraphAsset& out_compiled, std::string& out_error)
{
    out_compiled = CompiledGraphAsset{};
    out_compiled.name = graph.name;
    out_compiled.schemaVersion = graph.schemaVersion;
    out_compiled.resources = graph.resources;

    if (!validateUniqueNodeIds(graph, out_error)) return false;
    if (!validateUniqueEdgeIds(graph, out_error)) return false;
    if (!validateEdgeNodes(graph, out_error)) return false;

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

    return true;
}

}
