#pragma once
#include "../core/AssetDB.h"
#include "ResourceCache.h"
#include "MeshLoader.h"          // 读取 rawmesh
#include "TextureLoader.h"
#include "MaterialLoader.h"
#include "RawTypes.hpp"

namespace dk {




class ResourceLoader
{
public:
    ResourceLoader(const std::string& rawDir,
                   AssetDB&           db, ResourceCache& cache)
        : _dir(rawDir), _db(db), _cache(cache)
    {
    }

    std::shared_ptr<MeshData> loadMesh(UUID);
    std::shared_ptr<TextureData>  loadImage(UUID);
    std::shared_ptr<MaterialData> loadMaterial(UUID);

private:
    std::filesystem::path _dir;
    AssetDB&              _db;
    ResourceCache&        _cache;
};
}
