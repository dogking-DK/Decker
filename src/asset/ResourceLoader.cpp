#include "ResourceLoader.h"
#include "AssetHelpers.hpp"
#include "RawTypes.hpp"

namespace dk {
using namespace helper;


/* ---- Mesh ---- */
std::shared_ptr<MeshDataFlex> ResourceLoader::loadMesh(UUID id)
{
    return _cache.resolve<MeshDataFlex>(id, [&]
    {
        auto meta = _db.get(id).value();
        auto m    = load_raw_mesh_flex(_dir / meta.rawPath);

        /* ²ÄÖÊÒÀÀµ£¨¿ÉÑ¡£©£ºmeta.dependencies[0] = material uuid */
        //if (!meta.dependencies.empty())
        //m->material = loadMaterial(meta.dependencies[0]);
        return m;
    });
}

/* ---- Image ---- */
std::shared_ptr<TextureData> ResourceLoader::loadImage(UUID id)
{
    return _cache.resolve<TextureData>(id, [&]
    {
        auto meta   = _db.get(id).value();
        auto raw    = read_pod<RawImageHeader>(_dir / meta.rawPath);             // header
        auto pixels = read_blob<uint8_t>(_dir / meta.rawPath,
                                         sizeof(RawImageHeader), raw.width * raw.height * raw.channels);
        auto t = std::make_shared<TextureData>();
        *t     = {
            .w = static_cast<uint32_t>(raw.width),
            .h = static_cast<uint32_t>(raw.height),
            .d = static_cast<uint32_t>(raw.depth),
            .c = static_cast<uint32_t>(raw.channels),
            .pixels = std::move(pixels)
        };
        return t;
    });
}

/* ---- Material ---- */
std::shared_ptr<MaterialData> ResourceLoader::loadMaterial(UUID id)
{
    return _cache.resolve<MaterialData>(id, [&]
    {
        auto meta      = _db.get(id).value();
        auto raw       = read_pod<RawMaterial>(_dir / meta.rawPath);
        auto mat       = std::make_shared<MaterialData>();
        mat->metallic  = raw.metallicFactor;
        mat->roughness = raw.roughnessFactor;
        if (!raw.baseColorTexture.is_nil())
            mat->base_color_tex = loadImage(raw.baseColorTexture);
        return mat;
    });
}
}
