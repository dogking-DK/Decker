#include "ResourceLoader.h"

#include <fmt/base.h>

#include "AssetHelpers.hpp"
#include "RawTypes.hpp"
#include "MeshLoader.h"
#include "TextureLoader.h"
#include "MaterialLoader.h"

namespace dk {
using namespace helper;

namespace {
    std::shared_ptr<MeshData> decode_mesh_from_raw(const std::filesystem::path& path)
    {
        return load_mesh(path);
    }

    std::shared_ptr<TextureData> decode_image_from_raw(const std::filesystem::path& path)
    {
        auto raw    = read_pod<RawImageHeader>(path);             // header
        auto pixels = read_blob<uint8_t>(path, sizeof(RawImageHeader), raw.width * raw.height * raw.channels);
        auto t      = std::make_shared<TextureData>();
        *t          = {
            .width = static_cast<uint32_t>(raw.width),
            .height = static_cast<uint32_t>(raw.height),
            .depth = static_cast<uint32_t>(raw.depth),
            .channels = static_cast<uint32_t>(raw.channels),
            .pixels = std::move(pixels)
        };
        return t;
    }

    RawMaterial decode_material_from_raw(const std::filesystem::path& path)
    {
        return read_pod<RawMaterial>(path);
    }

    class MeshResourceLoader : public IResourceLoader<MeshData>
    {
    public:
        MeshResourceLoader(std::filesystem::path dir, AssetDB& db, ResourceLoader& owner)
            : _dir(std::move(dir)), _db(db), _owner(owner)
        {
        }

        std::shared_ptr<MeshData> load(UUID id) override
        {
            auto meta = _db.get(id);
            if (!meta)
            {
                fmt::print(stderr, "ResourceLoader: Mesh {} not found in DB\n", to_string(id));
                return std::shared_ptr<MeshData>{};
            }
            auto m = decode_mesh_from_raw(_dir / meta->raw_path);

            if (auto it = meta->dependencies.find(AssetDependencyType::Material); it != meta->dependencies.end())
            {
                m->material = _owner.load<MaterialData>(it->second);
            }
            return m;
            return {};
        }

    private:
        std::filesystem::path _dir;
        AssetDB&              _db;
        ResourceLoader&       _owner;
    };

    class TextureResourceLoader : public IResourceLoader<TextureData>
    {
    public:
        TextureResourceLoader(std::filesystem::path dir, AssetDB& db) : _dir(std::move(dir)), _db(db)
        {
        }

        std::shared_ptr<TextureData> load(UUID id) override
        {
            auto meta = _db.get(id);
            if (!meta)
            {
                fmt::print(stderr, "ResourceLoader: Image {} not found in DB\n", to_string(id));
                return std::shared_ptr<TextureData>{};
            }
            return decode_image_from_raw(_dir / meta->raw_path);
        }

    private:
        std::filesystem::path _dir;
        AssetDB&              _db;
    };

    class MaterialResourceLoader : public IResourceLoader<MaterialData>
    {
    public:
        MaterialResourceLoader(std::filesystem::path dir, AssetDB& db, ResourceLoader& owner)
            : _dir(std::move(dir)), _db(db), _owner(owner)
        {
        }

        std::shared_ptr<MaterialData> load(UUID id) override
        {
            auto meta = _db.get(id);
            if (!meta)
            {
                fmt::print(stderr, "ResourceLoader: Material {} not found in DB\n", to_string(id));
                return std::shared_ptr<MaterialData>{};
            }
            auto raw        = decode_material_from_raw(_dir / meta->raw_path);
            auto mat        = std::make_shared<MaterialData>();
            mat->metallic   = raw.metallic_factor;
            mat->roughness  = raw.roughness_factor;
            mat->base_color = raw.base_color;
            if (!raw.base_color_texture.is_nil())
                mat->base_color_tex = _owner.load<TextureData>(raw.base_color_texture);
            if (!raw.metal_rough_texture.is_nil())
                mat->metal_rough_tex = _owner.load<TextureData>(raw.metal_rough_texture);
            if (!raw.occlusion_texture.is_nil())
                mat->occlusion_tex = _owner.load<TextureData>(raw.occlusion_texture);
            if (!raw.normal_texture.is_nil())
                mat->normal_tex = _owner.load<TextureData>(raw.normal_texture);
            if (!raw.emissive_texture.is_nil())
                mat->emissive_tex = _owner.load<TextureData>(raw.emissive_texture);
            return mat;
        }

    private:
        std::filesystem::path _dir;
        AssetDB&              _db;
        ResourceLoader&       _owner;
    };
} // namespace

ResourceLoader::ResourceLoader(const std::string& rawDir, AssetDB& db, ResourceCache& cache)
    : _dir(rawDir), _db(db), _cache(cache)
{
    registerLoader<MeshData, MeshResourceLoader>(AssetType::Mesh, "Mesh", _dir, _db, *this);
    registerLoader<TextureData, TextureResourceLoader>(AssetType::Image, "Image", _dir, _db);
    registerLoader<MaterialData, MaterialResourceLoader>(AssetType::Material, "Material", _dir, _db, *this);
}
} // namespace dk
