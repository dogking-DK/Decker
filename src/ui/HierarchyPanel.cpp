#include "HierarchyPanel.h"

#include <fmt/base.h>

namespace dk {
void HierarchyPanel::onGui(const std::string& title)
{
    if (_roots == nullptr) return;
    const bool panel_open = ImGui::Begin(title.c_str());
    if (panel_open)
    {
        _node_clicked_this_frame = false;
        drawNode(*_roots);
        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !_node_clicked_this_frame)
        {
            _selected = nullptr;
        }
    }
    ImGui::End();
}

void HierarchyPanel::drawNode(SceneNode& n)
{
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;
    if (_selected == &n)
    {
        flags |= ImGuiTreeNodeFlags_Selected;
    }
    ImGui::PushID(&n);
    bool visible = n.visible;
    if (ImGui::Checkbox("##visible", &visible))
    {
        n.visible = visible;
        _node_clicked_this_frame = true;
    }
    ImGui::SameLine();
    bool               open = ImGui::TreeNodeEx(&n.id, flags, "%s", n.name.c_str());
    //bool               open  = ImGui::TreeNodeEx(&n.id, flags,
    //                              "%s  [%s]", n.name.c_str(),
    //                              to_string(n.id).c_str());
    ImGui::PopID();
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
    {
        _selected = &n;
        _node_clicked_this_frame = true;
        ImGui::SetItemDefaultFocus();               // 简单高亮演示
    }
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
    {
        //fmt::print("Double-clicked {} ({})\n", n.name.c_str(), to_string(n.id).c_str());
        fmt::print("Double-clicked {} ({})\n", n.name.c_str(), to_string(n.id).c_str());
    }

    if (open)
    {
        for (auto& ch : n.children)
        {
            drawNode(*ch);
        }
        ImGui::TreePop();
    }
}
}
