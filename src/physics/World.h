#pragma once
#include <vector>
#include <memory>
#include <tsl/robin_map.h>
#include <fmt/format.h>

#include "collider/ICollider.h"
#include "data/Particle.h"

namespace dk {
struct WorldSettings
{
    float fixed_dt{1.0f / 100.0f}; // 时间步长
    int   substeps{10}; // 每帧的子步数
};


class ISystem
{
public:
    virtual      ~ISystem() = default;
    virtual void step(float dt) = 0; // one substep

    // --- 新增的渲染数据导出接口 ---
/**
 * @brief 高效地填充一个用于渲染的 PointData 向量.
 * @param out_data 要被填充的目标向量. 函数内部会清空并重新填充它.
 */
    virtual void getRenderData(std::vector<PointData>& out_data) const = 0;
};


class World
{
public:
    explicit World(const WorldSettings& s);

    template <class T, class... Args>
    T* addSystem(const std::string& name, Args&&... args)
    {
        // 检查名称是否已经存在，防止意外覆盖
        if (systems_.count(name))
        {
            // 你可以选择如何处理：抛出异常、打印错误并返回nullptr，或者直接断言
            fmt::print(stderr, "Error: System with name '{}' already exists.\n", name);
            return nullptr;
        }
        auto ptr = std::make_unique<T>(std::forward<Args>(args)...);
        T*   raw = ptr.get();
        systems_.insert_or_assign(name, std::move(ptr));
        return raw;
    }

    ISystem* getSystem(const std::string_view& name)
    {
        auto it = systems_.find(std::string(name));
        return (it == systems_.end()) ? nullptr : it->second.get();
    }

    const ISystem* getSystem(const std::string_view& name) const
    {
        auto it = systems_.find(std::string(name));
        return (it == systems_.end()) ? nullptr : it->second.get();
    }

    template <class T>
        requires std::is_base_of_v<ISystem, T>
    T* getSystemAs(const std::string_view& name)
    {
        if (auto* base = getSystem(name))
        {
            return dynamic_cast<T*>(base);
        }
        return nullptr;
    }

    template <class T>
        requires std::is_base_of_v<ISystem, T>
    const T* getSystemAs(const std::string_view& name) const
    {
        if (auto* base = getSystem(name))
        {
            return dynamic_cast<T*>(base);
        }
        return nullptr;
    }
    void                 tick(float real_dt); // accumulate and run fixed steps
    const WorldSettings& settings() const { return settings_; }

private:
    void handleCollisions()
    {
        // 对每个系统的每个粒子进行碰撞检测和响应
        //for (auto& system : systems_) {
        //    ParticleData& data = system.second();
        //    const size_t particleCount = data.size();

        //    for (size_t i = 0; i < particleCount; ++i) {
        //        if (data.is_fixed[i]) continue;

        //        // 与世界中的所有碰撞体进行检测
        //        for (const auto& collider : colliders) {
        //            CollisionInfo info = collider->testCollision(data.position[i], 0.0f); // 假设粒子半径为0

        //            if (info.hasCollided) {
        //                // 如果碰撞，进行响应
        //                resolveCollision(data, i, info);
        //            }
        //        }
        //    }
        //}
    }

    void resolveCollision(ParticleData& data, size_t particleIndex, const CollisionInfo& info)
    {
        // 1. 位置修正 (投影)
        // 将粒子沿法线方向推出碰撞体
        data.position[particleIndex] += info.normal * info.penetrationDepth;

        // 2. 速度修正 (响应)
        // 计算沿法线方向的速度分量
        float velocityAlongNormal = dot(data.velocity[particleIndex], info.normal);

        // 如果速度是朝向碰撞体内部的，才需要响应
        if (velocityAlongNormal < 0)
        {
            // 定义恢复系数 (弹性)，0=完全非弹性, 1=完全弹性
            constexpr float restitution = 0.5f;

            // 计算需要施加的冲量，以反转法线方向的速度
            float j       = -(1.0f + restitution) * velocityAlongNormal;
            vec3  impulse = j * info.normal;

            // 应用冲量 (由于我们直接操作速度，所以是 impulse / mass，但这里简化为直接改变速度)
            data.velocity[particleIndex] += impulse;
        }
    }




    WorldSettings                                           settings_{};
    tsl::robin_map<std::string, std::unique_ptr<ISystem>>   systems_;
    tsl::robin_map<std::string, std::unique_ptr<ICollider>> _colliders;
    float                                                   acc_{0.f};
};
}
