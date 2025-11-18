#pragma once

#include <iostream>
#include <vector>
#include <memory>
#include <string>
#include <queue>
#include <algorithm>
#include <climits>

namespace dk {
struct RenderTaskBase;

// ---------------------------------------------
// 基本 ID 类型
// ---------------------------------------------
using ResourceId = uint32_t;
using TaskId     = uint32_t;

// 前置声明
class RenderGraph;
class RenderTaskBuilder;

// ---------------------------------------------
// 资源生命周期枚举
// ---------------------------------------------
enum class ResourceLifetime
{
    Transient,   // 每帧内创建/销毁（未来可接 image pool）
    External     // 外部管理（swapchain RT 等）
};

// ---------------------------------------------
// ResourceBase：所有资源的公共信息
// ---------------------------------------------
struct ResourceBase
{
    ResourceId       id = ~0u;
    std::string      name;
    ResourceLifetime lifetime = ResourceLifetime::Transient;

    RenderTaskBase*              creator = nullptr;
    std::vector<RenderTaskBase*> readers;
    std::vector<RenderTaskBase*> writers;

    // 编译阶段填充：
    int firstUse = -1;
    int lastUse  = -1;

    virtual ~ResourceBase() = default;

    virtual void realize()
    {
        // 这一版先打印，之后接 Vulkan / Pool
        std::cout << "[RG] Realize resource \"" << name << "\" (id=" << id << ")\n";
    }

    virtual void derealize()
    {
        std::cout << "[RG] Derealize resource \"" << name << "\" (id=" << id << ")\n";
    }
};

// ---------------------------------------------
// DummyDesc / DummyActual：占位类型
// 将来可以替换成 ImageDesc/Image 或 BufferDesc/Buffer
// ---------------------------------------------
struct DummyDesc
{
    int size = 0;   // 只是示意
};

struct DummyActual
{
    int dummy = 0;  // 只是示意
};

// ---------------------------------------------
// 模板 Resource：挂上描述和实际对象类型
// ---------------------------------------------
template <typename DescT, typename ActualT>
struct Resource : ResourceBase
{
    using Desc   = DescT;
    using Actual = ActualT;

    Desc                    desc;
    Actual*                 external = nullptr;           // External 时指向外部对象
    std::unique_ptr<Actual> actual;       // Transient 时使用

    Resource(const std::string& n, const Desc& d,
             ResourceLifetime   life = ResourceLifetime::Transient)
    {
        name     = n;
        desc     = d;
        lifetime = life;
    }

    Actual* get()
    {
        return external ? external : actual.get();
    }

    void realize() override
    {
        if (lifetime == ResourceLifetime::Transient && !actual)
        {
            actual = std::make_unique<Actual>();
        }
        std::cout << "[RG] Realize resource \"" << name << "\" (id=" << id << ")\n";
    }

    void derealize() override
    {
        if (lifetime == ResourceLifetime::Transient)
        {
            actual.reset();
        }
        std::cout << "[RG] Derealize resource \"" << name << "\" (id=" << id << ")\n";
    }
};
}
