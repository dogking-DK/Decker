#pragma once
#include "Importer.h"

namespace dk {
class GltfImporter final : public Importer {
public:
    std::optional<AssetNode> import(const std::filesystem::path& file) override;
};
REGISTER_IMPORTER(GltfImporter, "gltf");   // Ö§³Ö .gltf .glb
REGISTER_IMPORTER(GltfImporter, "glb");
}
