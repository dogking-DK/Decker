#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "AssetNode.hpp"
#include "AssetMeta.hpp"
#include "runtime/resource/cpu/ResourceCache.h"
#include "runtime/resource/cpu/ResourceLoader.h"
#include "scene/SceneTypes.h"

namespace dk {

/// @brief 场景构建接口，负责将 Asset 层结果转换为运行时节点。
class SceneBuilder
{
public:
    virtual ~SceneBuilder() = default;

    /// @brief 从 ImportResult 构建场景树（仅接口，不做具体解析）。
    virtual std::shared_ptr<SceneNode> build(const ImportResult& result) = 0;
};

/// @brief 运行时场景接口，对标 UE World / Unity Scene。
class Scene
{
public:
    virtual ~Scene() = default;

    /// @brief 设置场景根节点。
    virtual void setRoot(std::shared_ptr<SceneNode> root) = 0;

    /// @brief 访问场景根节点。
    virtual std::shared_ptr<SceneNode> getRoot() = 0;

    /// @brief 遍历场景节点（广度或深度遍历由实现决定）。
    virtual void forEachNode(const std::function<void(SceneNode&)>& visitor) = 0;
};

/// @brief 资源绑定接口，用于在场景层与资源层之间同步。
class SceneResourceBinder
{
public:
    virtual ~SceneResourceBinder() = default;

    /// @brief 预热 CPU 资源缓存，例如解析 MeshData / MaterialData。
    virtual void preloadCPU(Scene& scene, ResourceLoader& loader, ResourceCache& cache) = 0;

    /// @brief 预热 GPU 资源缓存（未来扩展）。
    virtual void preloadGPU(Scene& scene) = 0;
};

/// @brief 场景系统入口，负责生命周期管理。
class SceneSystem
{
public:
    virtual ~SceneSystem() = default;

    /// @brief 创建或切换当前场景。
    virtual std::shared_ptr<Scene> createScene(const std::string& name) = 0;

    /// @brief 获取当前场景。
    virtual std::shared_ptr<Scene> currentScene() = 0;

    /// @brief 每帧更新（动画/物理/脚本）。
    virtual void tick(double delta_time) = 0;
};

/// @brief 默认场景实现，负责节点存取与遍历。
class DefaultScene final : public Scene
{
public:
    explicit DefaultScene(std::string name);

    void setRoot(std::shared_ptr<SceneNode> root) override;
    std::shared_ptr<SceneNode> getRoot() override;
    void forEachNode(const std::function<void(SceneNode&)>& visitor) override;

    const std::string& name() const noexcept { return _name; }

private:
    std::string              _name;
    std::shared_ptr<SceneNode> _root;
};

/// @brief 从 ImportResult 生成场景树的默认实现。
class DefaultSceneBuilder final : public SceneBuilder
{
public:
    std::shared_ptr<SceneNode> build(const ImportResult& result) override;

private:
    static std::shared_ptr<SceneNode> cloneAssetNode(const std::shared_ptr<AssetNode>& node,
                                                     const std::shared_ptr<SceneNode>& parent);
};

/// @brief 简单的资源预热器，根据 raw 扩展推断资源类型并加载。
class DefaultSceneResourceBinder final : public SceneResourceBinder
{
public:
    explicit DefaultSceneResourceBinder(std::vector<AssetMeta> catalog = {});

    void preloadCPU(Scene& scene, ResourceLoader& loader, ResourceCache& cache) override;
    void preloadGPU(Scene& scene) override;

    /// 更新可用的资源目录。
    void setCatalog(std::vector<AssetMeta> catalog);

private:
    std::vector<AssetMeta> _catalog;
};

/// @brief 默认场景系统，持有 Builder / Binder 并管理当前场景生命周期。
class DefaultSceneSystem final : public SceneSystem
{
public:
    DefaultSceneSystem(std::unique_ptr<SceneBuilder>       builder,
                       std::unique_ptr<SceneResourceBinder> binder);

    std::shared_ptr<Scene> createScene(const std::string& name) override;
    std::shared_ptr<Scene> currentScene() override;
    void                   tick(double delta_time) override;

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

