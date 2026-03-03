#include "RenderGraphEditorPanel.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <imgui.h>
#include <imgui-node-editor/imgui_node_editor.h>

namespace dk::ui {
namespace ed = ax::NodeEditor;

namespace {
ed::NodeId makeNodeId(dk::GraphNodeId node_id)
{
    return ed::NodeId(static_cast<std::uintptr_t>(0x10000000ull + node_id));
}

ed::PinId makePinId(dk::GraphNodeId node_id, std::string_view pin_name, bool is_input)
{
    const std::uint64_t name_hash = static_cast<std::uint64_t>(std::hash<std::string_view>{}(pin_name));
    const std::uint64_t io_mask = is_input ? 0x1000000ull : 0x2000000ull;
    const std::uint64_t raw_id =
        0x200000000ull |
        (static_cast<std::uint64_t>(node_id) << 32) |
        io_mask |
        (name_hash & 0x00FFFFFFull);
    return ed::PinId(static_cast<std::uintptr_t>(raw_id));
}

ed::LinkId makeLinkId(dk::GraphEdgeId edge_id, std::size_t fallback_index)
{
    const std::uint64_t base = edge_id != 0 ? edge_id : static_cast<std::uint64_t>(fallback_index + 1);
    return ed::LinkId(static_cast<std::uintptr_t>(0x30000000ull + base));
}

void addUniquePin(std::vector<std::string>& pins, std::string_view pin_name)
{
    const auto it = std::find(pins.begin(), pins.end(), pin_name);
    if (it == pins.end())
    {
        pins.emplace_back(pin_name);
    }
}
}

RenderGraphEditorPanel::RenderGraphEditorPanel()
{
    ed::Config config{};
    config.SettingsFile = "RenderGraphNodeEditor.json";
    _editor_context = ed::CreateEditor(&config);
    _focus_pending = true;
}

RenderGraphEditorPanel::~RenderGraphEditorPanel()
{
    if (_editor_context)
    {
        ed::DestroyEditor(_editor_context);
        _editor_context = nullptr;
    }
}

void RenderGraphEditorPanel::draw(const GraphAsset* graph_asset)
{
    if (!_visible || !_editor_context)
    {
        return;
    }

    if (!ImGui::Begin("Render Graph", &_visible))
    {
        ImGui::End();
        return;
    }

    if (!graph_asset)
    {
        ImGui::TextUnformatted("Render graph snapshot is not available.");
        ImGui::End();
        return;
    }

    ImGui::Text("Name: %s", graph_asset->name.c_str());
    ImGui::Text("Nodes: %zu  Edges: %zu  Resources: %zu",
                graph_asset->nodes.size(),
                graph_asset->edges.size(),
                graph_asset->resources.size());
    if (ImGui::Button("Fit Graph"))
    {
        _focus_pending = true;
    }
    ImGui::Separator();

    std::unordered_map<GraphNodeId, std::vector<std::string>> input_pins;
    std::unordered_map<GraphNodeId, std::vector<std::string>> output_pins;
    input_pins.reserve(graph_asset->nodes.size());
    output_pins.reserve(graph_asset->nodes.size());

    for (const auto& edge : graph_asset->edges)
    {
        addUniquePin(output_pins[edge.fromNode], edge.fromPin);
        addUniquePin(input_pins[edge.toNode], edge.toPin);
    }

    ed::SetCurrentEditor(_editor_context);
    ed::Begin("RenderGraphNodeEditor");

    for (const auto& node : graph_asset->nodes)
    {
        const auto node_id = makeNodeId(node.id);
        auto& node_inputs = input_pins[node.id];
        auto& node_outputs = output_pins[node.id];

        ed::BeginNode(node_id);
        ImGui::Text("#%u  %s", node.id, node.type.c_str());
        ImGui::Text("params: %zu", node.params.size());
        ImGui::Separator();

        ImGui::BeginGroup();
        if (node_inputs.empty())
        {
            ImGui::TextUnformatted("in: (none)");
        }
        else
        {
            for (const auto& pin_name : node_inputs)
            {
                ed::BeginPin(makePinId(node.id, pin_name, true), ed::PinKind::Input);
                ImGui::Text("-> %s", pin_name.c_str());
                ed::EndPin();
            }
        }
        ImGui::EndGroup();

        ImGui::SameLine(0.0f, 28.0f);

        ImGui::BeginGroup();
        if (node_outputs.empty())
        {
            ImGui::TextUnformatted("out: (none)");
        }
        else
        {
            for (const auto& pin_name : node_outputs)
            {
                ed::BeginPin(makePinId(node.id, pin_name, false), ed::PinKind::Output);
                ImGui::Text("%s ->", pin_name.c_str());
                ed::EndPin();
            }
        }
        ImGui::EndGroup();
        ed::EndNode();
    }

    for (std::size_t i = 0; i < graph_asset->edges.size(); ++i)
    {
        const auto& edge = graph_asset->edges[i];
        ed::Link(
            makeLinkId(edge.id, i),
            makePinId(edge.fromNode, edge.fromPin, false),
            makePinId(edge.toNode, edge.toPin, true));
    }

    if (_focus_pending)
    {
        ed::NavigateToContent();
        _focus_pending = false;
    }

    ed::End();
    ed::SetCurrentEditor(nullptr);
    ImGui::End();
}

}
