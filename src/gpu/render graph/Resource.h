#pragma once

#include <iostream>
#include <vector>
#include <memory>
#include <string>
#include <queue>
#include <algorithm>
#include <climits>

#include "vk_base.h"
#include "vk_types.h"

namespace dk {
namespace vkcore {
    class VulkanContext;
}

// 前置声明
struct RenderTaskBase;

// 执行时要需要 VulkanContext & VMA，这里搞个简单的上下文
struct RenderGraphContext
{
    vkcore::VulkanContext* vkCtx      = nullptr;
    FrameData*             frame_data = nullptr;
    bool                   compiled_  = false;
};

// ---------------------------------------------
// 基本 ID 类型
// ---------------------------------------------
using ResourceId = uint32_t;
using TaskId     = uint32_t;

// ---------------------------------------------
// 资源生命周期枚举
// ---------------------------------------------
enum class ResourceLifetime : std::uint8_t
{
    Transient,    // 每帧内创建/销毁（未来可接 image pool）
    External,     // 外部管理（swapchain RT 等）
    Persistent,   // 持久性
};

// ---------------------------------------------
// ResourceBase：所有资源的公共信息
// ---------------------------------------------
class ResourceBase
{
public:
    virtual ~ResourceBase() = default;

    virtual void realize(RenderGraphContext& ctx)
    {
    }

    virtual void derealize(RenderGraphContext& ctx)
    {
        if (ctx.compiled_)
        {
            std::cout << "[RG] Derealize resource \"" << _name << "\" (id=" << _id << ")\n";
        }
    }

    std::string      name() const { return _name; }
    ResourceId       id() const { return _id; }
    ResourceLifetime lifetime() const { return _lifetime; }

protected:
    ResourceId       _id = ~0u;
    std::string      _name;
    ResourceLifetime _lifetime = ResourceLifetime::Transient;

    RenderTaskBase*              creator = nullptr;
    std::vector<RenderTaskBase*> readers;
    std::vector<RenderTaskBase*> writers;

    // 编译阶段填充：
    int firstUse = -1;
    int lastUse  = -1;
    friend class RenderTaskBuilder;
    friend class RenderGraph;
};

// ---------------------------------------------
// 模板 Resource：挂上描述和实际对象类型
// ---------------------------------------------
template <typename DescT, typename ActualT>
class Resource : public ResourceBase
{
public:
    using Desc   = DescT;
    using Actual = ActualT;

    Resource(const std::string& n, const Desc& d,
             ResourceLifetime   life = ResourceLifetime::Transient)
    {
        _name     = n;
        _desc     = d;
        _lifetime = life;
    }

    Actual* get()
    {
        return external ? external : actual.get();
    }

    const Desc& desc() { return _desc; }

    void realize(RenderGraphContext& ctx) override
    {
        if (_lifetime == ResourceLifetime::Transient && !actual)
        {
            actual = std::make_unique<Actual>();
        }
        std::cout << "[RG] Realize resource \"" << _name << "\" (id=" << _id << ")\n";
    }

    void derealize(RenderGraphContext& ctx) override
    {
        if (_lifetime == ResourceLifetime::Transient)
        {
            actual.reset();
        }
        std::cout << "[RG] Derealize resource \"" << _name << "\" (id=" << _id << ")\n";
    }

private:
    Desc                    _desc;
    Actual*                 external = nullptr; // External 时指向外部对象
    std::unique_ptr<Actual> actual;       // Transient 时使用
};
}
