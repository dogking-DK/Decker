#include "HierarchyPanel.h"

#include <fmt/base.h>

namespace dk {
void HierarchyPanel::onGui(const std::string& title)
{
    if (_roots == nullptr) return;
    const bool panel_open = ImGui::Begin(title.c_str());
    if (panel_open)
    {
        drawNode(*_roots);
    }
    ImGui::End();
}

void HierarchyPanel::drawNode(const SceneNode& n)
{
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;
    bool               open = ImGui::TreeNodeEx(&n.id, flags, "%s", n.name.c_str());
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
        ImGui::SetItemDefaultFocus();               // 简单高亮演示
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
