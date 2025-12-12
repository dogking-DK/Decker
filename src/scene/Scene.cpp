#include "Scene.h"

#include <algorithm>
#include <optional>
#include <stack>
#include <string_view>

#include "importer/Importer.h"
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
        return std::nullopt;
    }
} // namespace

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

void Scene::forEachNode(const std::function<void(SceneNode&)>& visitor)
{
    if (!visitor) return;
    traverse(_root, visitor);
}

std::shared_ptr<SceneNode> SceneBuilder::cloneAssetNode(const std::shared_ptr<AssetNode>& src,
                                                        const std::shared_ptr<SceneNode>& parent)
{
    auto dst      = std::make_shared<SceneNode>();
    dst->name     = src ? src->name : "Node";
    dst->id       = src ? src->id : uuid_from_string(dst->name);
    dst->asset_id = src ? std::optional<UUID>{src->id} : std::nullopt;
    dst->parent   = parent;

    if (src)
    {
        for (auto& child : src->children)
        {
            dst->children.push_back(cloneAssetNode(child, dst));
        }
    }
    return dst;
}

std::shared_ptr<SceneNode> SceneBuilder::build(const ImportResult& result)
{
    if (result.nodes.empty()) return {};

    if (result.nodes.size() == 1)
    {
        return cloneAssetNode(result.nodes.front(), {});
    }

    auto root      = std::make_shared<SceneNode>();
    root->name     = "SceneRoot";
    root->id       = uuid_from_string(root->name);
    root->asset_id = {};

    for (const auto& n : result.nodes)
    {
        root->children.push_back(cloneAssetNode(n, root));
    }
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
    for (const auto& meta : _catalog)
    {
        auto type = guess_asset_type(meta.raw_path);
        if (!type) continue;

        switch (*type)
        {
        case AssetType::Mesh: loader.load<MeshData>(meta.uuid);
            break;
        case AssetType::Image: loader.load<TextureData>(meta.uuid);
            break;
        case AssetType::Material: loader.load<MaterialData>(meta.uuid);
            break;
        }
    }
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
