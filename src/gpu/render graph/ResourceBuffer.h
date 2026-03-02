#pragma once

#include "Resource.h"
#include "Vulkan/Buffer.h"
#include "Vulkan/Context.h"

namespace dk {

struct BufferDesc
{
    uint64_t            size  = 0;
    vk::BufferUsageFlags usage = vk::BufferUsageFlagBits::eStorageBuffer;
    VmaMemoryUsage      memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;
};

class FrameGraphBuffer
{
public:
    vk::Buffer getVkBuffer() const
    {
        return buffer ? buffer->getHandle() : VK_NULL_HANDLE;
    }

    vkcore::BufferResource*       getBuffer() { return buffer.get(); }
    const vkcore::BufferResource* getBuffer() const { return buffer.get(); }

private:
    std::shared_ptr<vkcore::BufferResource> buffer{nullptr};
    friend class RGResource<BufferDesc, FrameGraphBuffer>;
};

template <>
class RGResource<BufferDesc, FrameGraphBuffer> : public RGResourceBase
{
public:
    using Desc   = BufferDesc;
    using Actual = FrameGraphBuffer;

    RGResource(const std::string& n, const Desc& d, ResourceLifetime life)
    {
        _name     = n;
        _desc     = d;
        _lifetime = life;
        _kind     = ResourceKind::Buffer;
    }

    Actual*     get() const { return actual.get(); }
    const Desc& desc() const { return _desc; }
    void        setExternal(const std::shared_ptr<vkcore::BufferResource>& external);

    void realize(RenderGraphContext& ctx) override;
    void derealize(RenderGraphContext& ctx) override;

private:
    Desc                    _desc{};
    std::unique_ptr<Actual> actual;
};

}

