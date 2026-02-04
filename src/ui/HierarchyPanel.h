// src/ui/HierarchyPanel.hpp
#pragma once

#include "SceneTypes.h"

namespace dk {
class HierarchyPanel
{
public:
    void setRoots(SceneNode* r) { _roots = r; }
    void onGui(const std::string& title = "Hierarchy");
    SceneNode* selectedNode() const { return _selected; }

private:
    void                                     drawNode(SceneNode& n);
    SceneNode* _roots{nullptr};
    SceneNode* _selected{nullptr};
    bool       _node_clicked_this_frame{false};
};
}
