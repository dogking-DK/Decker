#pragma once
#include "Importer.h"

namespace dk {
class GltfImporter final : public Importer
{
public:
    std::vector<std::shared_ptr<AssetNode>> import(const std::filesystem::path& file) override;
    [[nodiscard]] bool supportsExtension(const std::string& ext) const override;
    
};
REGISTER_IMPORTER(GltfImporter)

}
