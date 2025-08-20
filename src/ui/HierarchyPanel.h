// src/ui/HierarchyPanel.hpp
#pragma once
#include "../asset/AssetNode.hpp"
#include <imgui.h>
#include <vector>

namespace dk {
class HierarchyPanel
{
public:
    void setRoots(const std::vector<std::shared_ptr<AssetNode>>& r) { roots = r; }
    void onGui(const std::string& title = "Hierarchy");

private:
    void                                    drawNode(const std::shared_ptr<AssetNode>& n);
    std::vector<std::shared_ptr<AssetNode>> roots;
};
}
