#include "Scene.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <optional>
#include <queue>
#include <set>
#include <stack>
#include <string_view>
#include <unordered_set>

#include <nlohmann/json.hpp>
#include <fmt/core.h>

#include "importer/Importer.h"
#include "Prefab.hpp"
#include "resource/cpu/MaterialLoader.h"
#include "resource/cpu/MeshLoader.h"
#include "resource/cpu/TextureLoader.h"

namespace dk {
namespace {
    /// @brief 深度优先遍历工具。
    template <class Fn>
    void traverse(const std::shared_ptr<SceneNode>& root, Fn&& fn)
    {
        if (!root) return;

        std::stack<std::shared_ptr<SceneNode>> stack;
        stack.push(root);
        while (!stack.empty())
        {
            auto node = stack.top();
            stack.pop();
            fn(*node);

            for (auto it = node->children.rbegin(); it != node->children.rend(); ++it)
                stack.push(*it);
        }
    }

    std::optional<AssetType> guess_asset_type(std::string_view raw_path)
    {
        if (raw_path.ends_with(".rawmesh")) return AssetType::Mesh;
        if (raw_path.ends_with(".rawimg")) return AssetType::Image;
        if (raw_path.ends_with(".rawmat")) return AssetType::Material;
        if (raw_path.ends_with(".prefab")) return AssetType::Prefab;
        return std::nullopt;
    }

    std::optional<AssetType> resolve_asset_type(const AssetMeta& meta)
    {
        if (meta.asset_type) return meta.asset_type;
        return guess_asset_type(meta.raw_path);
    }

    std::vector<PrefabNode> load_prefabs(const ImportResult& result)
    {
        std::vector<PrefabNode> prefabs;
        const auto              base_dir = result.raw_dir.empty() ? std::filesystem::path("cache/raw") : result.raw_dir;

        for (const auto& meta : result.metas)
        {
            const bool is_prefab = (meta.asset_type && *meta.asset_type == AssetType::Prefab) ||
                                   meta.raw_path.ends_with(".prefab");
            if (!is_prefab) continue;

            const auto    path = base_dir / meta.raw_path;
            std::ifstream is(path);
            if (!is) continue;

            nlohmann::json j;
            is >> j;
            prefabs.push_back(j.get<PrefabNode>());
        }

        return prefabs;
    }

    std::shared_ptr<SceneNode> clonePrefabNode(const PrefabNode&                 src,
                                               const std::shared_ptr<SceneNode>& parent,
                                               std::unordered_set<UUID>&         runtime_ids)
    {
        auto dst      = std::make_shared<SceneNode>();
        dst->name     = src.name.empty() ? "Node" : src.name;
        dst->id       = uuid_generate();
        dst->asset_id = src.id;
        dst->parent   = parent;
        dst->local_transform = src.local_transform;
        auto node_id1 = to_string(src.id);

        const auto [_, inserted] = runtime_ids.insert(dst->id);
        assert(inserted && "Prefab instance should have a unique runtime UUID");

        if (src.kind == AssetKind::Primitive)
        {
            auto& mesh_component = dst->addComponent<MeshInstanceComponent>();
            mesh_component.mesh_asset = src.id;
            mesh_component.material_asset = src.material_id;
        }

        for (const auto& child : src.children)
        {
            dst->children.push_back(clonePrefabNode(child, dst, runtime_ids));
        }
        return dst;
    }
}

Scene::Scene(std::string name) : _name(std::move(name))
{
}

void Scene::setRoot(std::shared_ptr<SceneNode> root)
{
    _root = std::move(root);
}

std::shared_ptr<SceneNode> Scene::getRoot()
{
    return _root;
}

std::shared_ptr<SceneNode> Scene::getRoot() const
{
    return _root;
}

void Scene::forEachNode(const std::function<void(SceneNode&)>& visitor)
{
    if (!visitor) return;
    traverse(_root, visitor);
}

std::shared_ptr<SceneNode> SceneBuilder::build(const ImportResult& result)
{
    auto prefabs = load_prefabs(result);
    std::queue<PrefabNode> prefabs_queue;
    prefabs_queue.emplace(prefabs.front());
    fmt::print("-------------------------scene node recurse info--------------------\n");
    while (!prefabs_queue.empty())
    {
        auto node = prefabs_queue.back();
        prefabs_queue.pop();
        fmt::print("{}: {}\n", node.name.c_str(), to_string(node.id).c_str());
        for (auto child : node.children)
        {
            prefabs_queue.emplace(child);
        }
    }
    if (!prefabs.empty())
    {
        std::unordered_set<UUID> runtime_ids;
        if (prefabs.size() == 1)
        {
            return clonePrefabNode(prefabs.front(), {}, runtime_ids);
        }

        auto root      = std::make_shared<SceneNode>();
        root->name     = "SceneRoot";
        root->id       = uuid_parse(root->name);
        root->asset_id = {};
        runtime_ids.insert(root->id);

        for (const auto& prefab : prefabs)
        {
            root->children.push_back(clonePrefabNode(prefab, root, runtime_ids));
        }
        return root;
    }

    auto root      = std::make_shared<SceneNode>();
    root->name     = "SceneRoot";
    root->id       = uuid_parse(root->name);
    root->asset_id = {};

    return root;
}

SceneResourceBinder::SceneResourceBinder(std::vector<AssetMeta> catalog)
    : _catalog(std::move(catalog))
{
}

void SceneResourceBinder::preloadCPU(Scene& scene, ResourceLoader& loader, ResourceCache& cache)
{
    (void)scene;
    (void)cache; // 缓存由 loader 持有的 ResourceCache 注入
    std::unordered_map<AssetType, std::vector<UUID>> batch_ids;
    for (const auto& meta : _catalog)
    {
        auto type = resolve_asset_type(meta);
        if (!type)
        {
            fmt::print(stderr, "SceneResourceBinder: unrecognized asset type for {} (raw_path={})\n",
                       to_string(meta.uuid), meta.raw_path);
            continue;
        }

        batch_ids[*type].push_back(meta.uuid);
    }

    if (auto it = batch_ids.find(AssetType::Mesh); it != batch_ids.end())
        loader.loadBatch<MeshData>(it->second);
    if (auto it = batch_ids.find(AssetType::Image); it != batch_ids.end())
        loader.loadBatch<TextureData>(it->second);
    if (auto it = batch_ids.find(AssetType::Material); it != batch_ids.end())
        loader.loadBatch<MaterialData>(it->second);
}

void SceneResourceBinder::preloadGPU(Scene& scene)
{
    (void)scene;
    // GPU 资源管理尚未接入，留空实现便于后续扩展。
}

void SceneResourceBinder::setCatalog(std::vector<AssetMeta> catalog)
{
    _catalog = std::move(catalog);
}

SceneSystem::SceneSystem(std::unique_ptr<SceneBuilder>        builder,
                         std::unique_ptr<SceneResourceBinder> binder)
    : _builder(std::move(builder)), _binder(std::move(binder))
{
}

std::shared_ptr<Scene> SceneSystem::createScene(const std::string& name)
{
    _current = std::make_shared<Scene>(name);
    return _current;
}

std::shared_ptr<Scene> SceneSystem::currentScene()
{
    return _current;
}

void SceneSystem::tick(double)
{
    // 预留：动画、脚本、物理等逐帧更新逻辑
}

std::shared_ptr<Scene> SceneSystem::buildSceneFromImport(const std::string&  name,
                                                         const ImportResult& result)
{
    if (!_builder) return {};

    fmt::print("-------------------------scene build info--------------------\n");
    for (auto meta : result.metas)
    {
        fmt::print("{}: {}\n", to_string(meta.asset_type.value()), to_string(meta.uuid));
    }

    auto scene = std::make_shared<Scene>(name);
    scene->setRoot(_builder->build(result));
    _current = scene;

    if (_binder)
    {
        _binder->setCatalog(result.metas);
    }

    return _current;
}

void SceneSystem::preloadResources(ResourceLoader& loader, ResourceCache& cache)
{
    if (_binder && _current)
    {
        _binder->preloadCPU(*_current, loader, cache);
        _binder->preloadGPU(*_current);
    }
}
} // namespace dk
