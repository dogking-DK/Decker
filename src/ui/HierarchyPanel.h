// src/ui/HierarchyPanel.hpp
#pragma once
#include "Prefab.hpp"
#include <imgui.h>
#include <vector>

namespace dk {
class HierarchyPanel
{
public:
    void setRoots(std::vector<PrefabNode>* r) { _roots = r; }
    void onGui(const std::string& title = "Hierarchy");

private:
    void                                     drawNode(const PrefabNode& n);
    std::vector<PrefabNode>* _roots{nullptr};
};
}
