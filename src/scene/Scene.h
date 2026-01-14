#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "AssetMeta.hpp"
#include "resource/cpu/ResourceCache.h"
#include "resource/cpu/ResourceLoader.h"
#include "SceneTypes.h"

namespace dk {
struct ImportResult;

/// @brief 默认场景实现，负责节点存取与遍历。
class Scene
{
public:
    explicit Scene(std::string name);

    void                       setRoot(std::shared_ptr<SceneNode> root);
    std::shared_ptr<SceneNode> getRoot();
    std::shared_ptr<SceneNode> getRoot() const;
    void                       forEachNode(const std::function<void(SceneNode&)>& visitor);

    const std::string& name() const noexcept { return _name; }

private:
    std::string                _name;
    std::shared_ptr<SceneNode> _root;
};

/// @brief 从 ImportResult 生成场景树的默认实现。
class SceneBuilder
{
public:
    std::shared_ptr<SceneNode> build(const ImportResult& result);
};

/// @brief 简单的资源预热器，根据 raw 扩展推断资源类型并加载。
class SceneResourceBinder
{
public:
    explicit SceneResourceBinder(std::vector<AssetMeta> catalog = {});

    void preloadCPU(Scene& scene, ResourceLoader& loader, ResourceCache& cache);
    void preloadGPU(Scene& scene);

    /// 更新可用的资源目录。
    void setCatalog(std::vector<AssetMeta> catalog);

private:
    std::vector<AssetMeta> _catalog;
};

/// @brief 默认场景系统，持有 Builder / Binder 并管理当前场景生命周期。
class SceneSystem
{
public:
    SceneSystem(std::unique_ptr<SceneBuilder>        builder,
                std::unique_ptr<SceneResourceBinder> binder);

    std::shared_ptr<Scene> createScene(const std::string& name);
    std::shared_ptr<Scene> currentScene();
    void                   tick(double delta_time);

    /// 便捷接口：直接从 ImportResult 创建并设置当前场景。
    std::shared_ptr<Scene> buildSceneFromImport(const std::string& name, const ImportResult& result);

    /// 预热 CPU / GPU 资源。
    void preloadResources(ResourceLoader& loader, ResourceCache& cache);

private:
    std::unique_ptr<SceneBuilder>        _builder;
    std::unique_ptr<SceneResourceBinder> _binder;
    std::shared_ptr<Scene>               _current;
};
} // namespace dk
