#pragma once

#include <filesystem>
#include <string>

#include "GraphAsset.h"

namespace dk {

bool loadGraphAssetFromJsonFile(const std::filesystem::path& path, GraphAsset& out_graph, std::string& out_error);
bool loadGraphAssetFromJsonText(const std::string& json_text, GraphAsset& out_graph, std::string& out_error);
// Persist graph asset as a formatted JSON file for tooling/editor round-trip.
bool saveGraphAssetToJsonFile(const std::filesystem::path& path, const GraphAsset& graph, std::string& out_error);
// Persist graph asset into JSON text for scripting and tests.
bool saveGraphAssetToJsonText(const GraphAsset& graph, std::string& out_json_text, std::string& out_error);

}
