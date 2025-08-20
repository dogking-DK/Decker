#pragma once
#include "Importer.h"

namespace dk {
class GltfImporter final : public Importer
{
public:
    ImportResult import(const std::filesystem::path& file, const ImportOptions& opts) override;
    [[nodiscard]] bool supportsExtension(const std::string& ext) const override;
    
};
REGISTER_IMPORTER(GltfImporter)

}
