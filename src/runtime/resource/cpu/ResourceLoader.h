#pragma once
#include <filesystem>
#include <memory>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <utility>

#include <fmt/base.h>

#include "AssetDB.h"
#include "ResourceCache.h"

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
    ~IResourceLoader() override = default;
    virtual std::shared_ptr<Res> load(UUID id) = 0;
};

class ResourceLoader
{
public:
    ResourceLoader(const std::string& rawDir, AssetDB& db, ResourceCache& cache);

    template <typename Res>
    std::shared_ptr<Res> load(UUID id);

private:
    struct RegisteredType
    {
        AssetType       type;
        std::string     name;
        std::type_index type_id;
    };

    template <typename Res, typename Loader, typename... Args>
    void registerLoader(AssetType type, std::string name, Args&&... args);

    template <typename Res>
    RegisteredType* registeredType();

    std::filesystem::path                                               _dir;
    AssetDB&                                                            _db;
    ResourceCache&                                                      _cache;
    std::unordered_map<AssetType, std::unique_ptr<IResourceLoaderBase>> _loaders;
    std::unordered_map<std::type_index, RegisteredType>                 _typeRegistry;
};

template <typename Res>
std::shared_ptr<Res> ResourceLoader::load(UUID id)
{
    auto* info = registeredType<Res>();
    if (!info) return {};

    auto it = _loaders.find(info->type);
    if (it == _loaders.end())
    {
        fmt::print(stderr, "ResourceLoader: loader for type {} ({}) missing\n", info->name, typeid(Res).name());
        return {};
    }

    auto* loader = dynamic_cast<IResourceLoader<Res>*>(it->second.get());
    if (!loader)
    {
        fmt::print(stderr, "ResourceLoader: loader for type {} ({}) has incompatible interface\n", info->name,
                   typeid(Res).name());
        return {};
    }

    return _cache.resolve<Res>(id, [&] { return loader->load(id); });
}

template <typename Res, typename Loader, typename... Args>
void ResourceLoader::registerLoader(AssetType type, std::string name, Args&&... args)
{
    auto type_id = std::type_index(typeid(Res));
    _typeRegistry.emplace(type_id, RegisteredType{type, std::move(name), type_id});
    _loaders.emplace(type, std::make_unique<Loader>(std::forward<Args>(args)...));
}

template <typename Res>
ResourceLoader::RegisteredType* ResourceLoader::registeredType()
{
    auto type_id = std::type_index(typeid(Res));
    auto it      = _typeRegistry.find(type_id);
    if (it == _typeRegistry.end())
    {
        fmt::print(stderr, "ResourceLoader: loader for type {} not registered\n", typeid(Res).name());
        return nullptr;
    }
    return &it->second;
}
} // namespace dk
