#pragma once
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <glm/vec3.hpp>

#include "BVH/AABB.hpp"
#include "Transform.h"
#include "UUID.hpp"

namespace dk {
/// @brief 场景组件基类，对标 Unity Component / UE ActorComponent。
class SceneComponent
{
public:
    virtual ~SceneComponent() = default;
};

/// @brief 网格实例组件，承载 CPU/GPU 资源句柄。
struct MeshInstanceComponent final : SceneComponent
{
    UUID mesh_asset;     ///< 对应 MeshData 的 Asset UUID。
    UUID material_asset; ///< 对应 Material 的 Asset UUID。
};

/// @brief 相机组件，负责渲染视口。
struct CameraComponent final : SceneComponent
{
    float vertical_fov = 60.0f;
    float near_plane   = 0.01f;
    float far_plane    = 1000.0f;
};

/// @brief 光源组件，对标 UE Light / Filament LightInstance。
struct LightComponent final : SceneComponent
{
    enum class Type
    {
        Directional,
        Point,
        Spot,
    };

    Type      type       = Type::Directional;
    glm::vec3 color      = glm::vec3(1.0f);
    float     intensity  = 1.0f; ///< 物理单位留待实现时确定
    float     range      = 10.0f;
    float     inner_cone = 0.0f;
    float     outer_cone = 0.0f;
};

/// @brief 环境组件，描述天空盒或 IBL 入口。
struct EnvironmentComponent final : SceneComponent
{
    UUID skybox_asset{}; ///< 可选：绑定的环境贴图资源。
};

/// @brief 包围盒组件，提供节点的本地空间 AABB。
struct BoundsComponent final : SceneComponent
{
    AABB local_bounds{};
};

/// @brief 场景节点，连接资源层与场景层。
struct SceneNode
{
    UUID                                         id{};            ///< 运行时节点句柄。
    std::string                                  name;            ///< 便于调试的名称。
    Transform                                    local_transform; ///< 与父节点的变换关系。
    bool                                         visible{true};
    std::weak_ptr<SceneNode>                     parent;
    std::vector<std::shared_ptr<SceneNode>>      children;
    std::vector<std::unique_ptr<SceneComponent>> components;

    /// 与 Asset 层的关联，便于从 AssetNode 回写或重建。
    std::optional<UUID> asset_id;

    // --- 新增辅助函数 ---

    // 添加组件：node.addComponent<MeshInstanceComponent>();
    template <typename T, typename... Args>
    T& addComponent(Args&&... args)
    {
        // 实际项目中可能需要检查是否已存在同类组件
        auto component = std::make_unique<T>(std::forward<Args>(args)...);
        T* raw_ptr = component.get();
        components.push_back(std::move(component));
        return *raw_ptr;
    }

    // 获取组件：node.getComponent<MeshInstanceComponent>();
    template <typename T>
    T* getComponent() const
    {
        for (const auto& comp : components)
        {
            // 使用 dynamic_cast 进行安全转换
            if (auto ptr = dynamic_cast<T*>(comp.get()))
            {
                return ptr;
            }
        }
        return nullptr;
    }
};
} // namespace dk
