#pragma once

#include <filesystem>
#include <string>

#include "GraphAsset.h"

namespace dk {

// Export graph asset as Graphviz DOT text for node-based visualization.
bool saveGraphAssetToDotText(const GraphAsset& graph, std::string& out_dot_text, std::string& out_error);
// Export graph asset as Graphviz DOT file.
bool saveGraphAssetToDotFile(const GraphAsset& graph, const std::filesystem::path& path, std::string& out_error);

}
