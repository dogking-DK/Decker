#pragma once
#include <filesystem>
#include <memory>
#include <unordered_map>
#include <utility>

#include "AssetDB.h"
#include "MaterialLoader.h"
#include "MeshLoader.h"          // 读取 rawmesh
#include "RawTypes.hpp"
#include "ResourceCache.h"
#include "TextureLoader.h"

namespace dk {

enum class AssetType
{
    Mesh,
    Image,
    Material,
};

class IResourceLoaderBase
{
public:
    virtual ~IResourceLoaderBase() = default;
};

template <typename Res>
class IResourceLoader : public IResourceLoaderBase
{
public:
    virtual ~IResourceLoader() = default;
    virtual std::shared_ptr<Res> load(UUID id) = 0;
};

namespace detail {
template <typename Res>
struct AssetTypeTraits
{
    static constexpr bool defined = false;
};

template <>
struct AssetTypeTraits<MeshData>
{
    static constexpr bool   defined = true;
    static constexpr AssetType type = AssetType::Mesh;
};

template <>
struct AssetTypeTraits<TextureData>
{
    static constexpr bool   defined = true;
    static constexpr AssetType type = AssetType::Image;
};

template <>
struct AssetTypeTraits<MaterialData>
{
    static constexpr bool   defined = true;
    static constexpr AssetType type = AssetType::Material;
};
} // namespace detail

class ResourceLoader
{
public:
    ResourceLoader(const std::string& rawDir, AssetDB& db, ResourceCache& cache);

    template <typename Res>
    std::shared_ptr<Res> load(UUID id);

private:
    template <typename Res>
    static constexpr AssetType assetTypeFor();

    template <typename Res, typename Loader, typename... Args>
    void registerLoader(Args&&... args);

    std::filesystem::path _dir;
    AssetDB&              _db;
    ResourceCache&        _cache;
    std::unordered_map<AssetType, std::unique_ptr<IResourceLoaderBase>> _loaders;
};

template <typename Res>
constexpr AssetType ResourceLoader::assetTypeFor()
{
    static_assert(detail::AssetTypeTraits<Res>::defined, "Asset type not registered for this resource");
    return detail::AssetTypeTraits<Res>::type;
}

template <typename Res>
std::shared_ptr<Res> ResourceLoader::load(UUID id)
{
    auto it = _loaders.find(assetTypeFor<Res>());
    if (it == _loaders.end()) return {};

    auto* loader = dynamic_cast<IResourceLoader<Res>*>(it->second.get());
    if (!loader) return {};

    return _cache.resolve<Res>(id, [&] { return loader->load(id); });
}

template <typename Res, typename Loader, typename... Args>
void ResourceLoader::registerLoader(Args&&... args)
{
    _loaders.emplace(assetTypeFor<Res>(), std::make_unique<Loader>(std::forward<Args>(args)...));
}

} // namespace dk
