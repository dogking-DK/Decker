// src/ui/HierarchyPanel.hpp
#pragma once

#include "SceneTypes.h"

namespace dk {
class HierarchyPanel
{
public:
    void setRoots(SceneNode* r) { _roots = r; }
    void onGui(const std::string& title = "Hierarchy");

private:
    void                                     drawNode(const SceneNode& n);
    SceneNode* _roots{nullptr};
};
}
