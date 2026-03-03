#pragma once

#include "render graph/GraphAsset.h"

namespace ax::NodeEditor
{
    struct EditorContext;
}

namespace dk::ui {

// ImGui node-editor panel for visualizing GraphAsset nodes and links.
class RenderGraphEditorPanel
{
public:
    RenderGraphEditorPanel();
    ~RenderGraphEditorPanel();

    bool visible() const { return _visible; }
    void setVisible(bool visible) { _visible = visible; }
    void requestFocus() { _focus_pending = true; }
    void draw(const GraphAsset* graph_asset);

private:
    bool _visible{true};
    bool _focus_pending{true};
    ax::NodeEditor::EditorContext* _editor_context{nullptr};
};

}
