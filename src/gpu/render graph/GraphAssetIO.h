#pragma once

#include <filesystem>
#include <string>

#include "GraphAsset.h"

namespace dk {

bool loadGraphAssetFromJsonFile(const std::filesystem::path& path, GraphAsset& out_graph, std::string& out_error);
bool loadGraphAssetFromJsonText(const std::string& json_text, GraphAsset& out_graph, std::string& out_error);

}

