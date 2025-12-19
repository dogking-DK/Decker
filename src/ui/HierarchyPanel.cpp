#include "HierarchyPanel.h"

#include <fmt/base.h>

namespace dk {
void HierarchyPanel::onGui(const std::string& title)
{
    ImGui::Begin(title.c_str());
    for (auto& r : *_roots) drawNode(r);
    ImGui::End();
}

void HierarchyPanel::drawNode(const PrefabNode& n)
{
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;
    //bool               open  = ImGui::TreeNodeEx(&n->id, flags,
    //                              "%s  [%s]", n->name.c_str(),
    //                              to_string(n->id).c_str());
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
            drawNode(ch);
        }
        ImGui::TreePop();
    }
}
}
