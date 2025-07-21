#include "ResourceLoader.h"

#include <fmt/base.h>

#include "AssetHelpers.hpp"
#include "RawTypes.hpp"

namespace dk {
using namespace helper;


/* ---- Mesh ---- */
std::shared_ptr<MeshData> ResourceLoader::loadMesh(UUID id)
{
    return _cache.resolve<MeshData>(id, [&]
    {
        auto meta = _db.get(id).value();
        auto m    = load_raw_mesh(_dir / meta.raw_path);

        /* 材质依赖（可选）：meta.dependencies[0] = material uuid */
        if (meta.dependencies.contains("material"))
        {
            const auto& mat_uuid = meta.dependencies.at("material");
            m->material = loadMaterial(meta.dependencies.at("material"));
        }
        return m;
    });
}

/* ---- Image ---- */
std::shared_ptr<TextureData> ResourceLoader::loadImage(UUID id)
{
    return _cache.resolve<TextureData>(id, [&]
    {
        auto meta   = _db.get(id).value();
        auto raw    = read_pod<RawImageHeader>(_dir / meta.raw_path);             // header
        auto pixels = read_blob<uint8_t>(_dir / meta.raw_path,
                                         sizeof(RawImageHeader), raw.width * raw.height * raw.channels);
        auto t = std::make_shared<TextureData>();
        *t     = {
            .width = static_cast<uint32_t>(raw.width),
            .height = static_cast<uint32_t>(raw.height),
            .depth = static_cast<uint32_t>(raw.depth),
            .channels = static_cast<uint32_t>(raw.channels),
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
        if (_db.get(id) == std::nullopt)
        {
            fmt::print(stderr, "ResourceLoader: Material {} not found in DB\n", to_string(id));
            return std::shared_ptr<MaterialData>{};
        }
        auto meta      = _db.get(id).value();
        auto raw       = read_pod<RawMaterial>(_dir / meta.raw_path);
        auto mat       = std::make_shared<MaterialData>();
        mat->metallic  = raw.metallic_factor;
        mat->roughness = raw.roughness_factor;
        if (!raw.base_color_texture.is_nil())
            mat->base_color_tex = loadImage(raw.base_color_texture);
        if (!raw.metal_rough_texture.is_nil())
            mat->metal_rough_tex = loadImage(raw.metal_rough_texture);
        if (!raw.occlusion_texture.is_nil())
            mat->occlusion_tex = loadImage(raw.occlusion_texture);
        if (!raw.normal_texture.is_nil())
            mat->normal_tex = loadImage(raw.normal_texture);
        if (!raw.emissive_texture.is_nil())
            mat->emissive_tex = loadImage(raw.emissive_texture);
        return mat;
    });
}
}
