#pragma once
#include "../core/AssetDB.h"
#include "ResourceCache.h"
#include "MeshLoaderFlex.h"          // ∂¡»° rawmesh
#include "RawTypes.hpp"

namespace dk {
struct TextureData
{
    uint32_t             w, h, d, c;
    std::vector<uint8_t> pixels;
};

struct MaterialData
{
    float                        metallic, roughness;
    std::shared_ptr<TextureData> base_color_tex;
};

class ResourceLoader
{
public:
    ResourceLoader(const std::string& rawDir,
                   AssetDB&           db, ResourceCache& cache)
        : _dir(rawDir), _db(db), _cache(cache)
    {
    }

    std::shared_ptr<MeshDataFlex> loadMesh(UUID);
    std::shared_ptr<TextureData>  loadImage(UUID);
    std::shared_ptr<MaterialData> loadMaterial(UUID);

private:
    std::filesystem::path _dir;
    AssetDB&              _db;
    ResourceCache&        _cache;
};
}
