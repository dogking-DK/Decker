#include "GraphAssetViz.h"

#include <fstream>
#include <sstream>
#include <unordered_map>

namespace dk {
namespace {

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

std::string escapeDot(std::string_view text)
{
    std::string out;
    out.reserve(text.size());
    for (const char ch : text)
    {
        if (ch == '\\')
        {
            out += "\\\\";
            continue;
        }
        if (ch == '"')
        {
            out += "\\\"";
            continue;
        }
        if (ch == '\n' || ch == '\r')
        {
            out += "\\n";
            continue;
        }
        out.push_back(ch);
    }
    return out;
}

std::string pathToString(const std::filesystem::path& path)
{
    return path.string();
}

}

bool saveGraphAssetToDotText(const GraphAsset& graph, std::string& out_dot_text, std::string& out_error)
{
    try
    {
        std::ostringstream ss;
        ss << "digraph RenderGraph {\n";
        ss << "  rankdir=LR;\n";
        ss << "  graph [fontsize=11, fontname=\"Consolas\", label=\""
           << escapeDot(graph.name.empty() ? "RenderGraph" : graph.name)
           << "\", labelloc=t];\n";
        ss << "  node [fontname=\"Consolas\", fontsize=10, shape=record];\n";
        ss << "  edge [fontname=\"Consolas\", fontsize=9];\n\n";

        ss << "  subgraph cluster_resources {\n";
        ss << "    label=\"Resources\";\n";
        ss << "    style=rounded;\n";
        ss << "    color=\"#9AA4B2\";\n";

        std::unordered_map<std::string, std::size_t> resource_index_by_name;
        resource_index_by_name.reserve(graph.resources.size());

        for (std::size_t i = 0; i < graph.resources.size(); ++i)
        {
            const auto& resource = graph.resources[i];
            resource_index_by_name.emplace(resource.name, i);
            ss << "    r_" << i << " [shape=box, style=\"rounded,filled\", fillcolor=\"#EEF2FF\", label=\""
               << escapeDot(resource.name)
               << "\\n" << toResourceKindText(resource.kind)
               << " / " << toResourceLifetimeText(resource.lifetime)
               << "\"];\n";
        }
        ss << "  }\n\n";

        for (const auto& node : graph.nodes)
        {
            ss << "  n_" << node.id
               << " [style=\"rounded,filled\", fillcolor=\"#E8F5E9\", label=\"{#"
               << node.id
               << "|"
               << escapeDot(node.type)
               << "|params: "
               << node.params.size()
               << "}\"];\n";
        }
        ss << "\n";

        for (const auto& edge : graph.edges)
        {
            ss << "  n_" << edge.fromNode
               << " -> n_" << edge.toNode
               << " [label=\""
               << escapeDot(edge.fromPin)
               << " -> "
               << escapeDot(edge.toPin)
               << "\"];\n";
        }
        ss << "\n";

        // If a string param directly references a resource name, draw a dashed helper edge.
        for (const auto& node : graph.nodes)
        {
            for (const auto& param : node.params)
            {
                if (param.type != ParamType::String)
                {
                    continue;
                }
                const auto* text = std::get_if<std::string>(&param.value);
                if (!text)
                {
                    continue;
                }
                const auto resource_it = resource_index_by_name.find(*text);
                if (resource_it == resource_index_by_name.end())
                {
                    continue;
                }
                ss << "  r_" << resource_it->second
                   << " -> n_" << node.id
                   << " [style=dashed, color=\"#6B7280\", label=\""
                   << escapeDot(param.name)
                   << "\"];\n";
            }
        }

        ss << "}\n";
        out_dot_text = ss.str();
        return true;
    }
    catch (const std::exception& e)
    {
        out_error = e.what();
        return false;
    }
}

bool saveGraphAssetToDotFile(const GraphAsset& graph, const std::filesystem::path& path, std::string& out_error)
{
    std::string dot_text;
    if (!saveGraphAssetToDotText(graph, dot_text, out_error))
    {
        return false;
    }

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out.is_open())
    {
        out_error = "Failed to open graph dot file for write: " + pathToString(path);
        return false;
    }
    out << dot_text;
    if (!out.good())
    {
        out_error = "Failed to write graph dot file: " + pathToString(path);
        return false;
    }
    return true;
}

}
