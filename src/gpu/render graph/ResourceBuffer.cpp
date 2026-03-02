#include "ResourceBuffer.h"

namespace dk {

void RGResource<BufferDesc, FrameGraphBuffer>::setExternal(
    const std::shared_ptr<vkcore::BufferResource>& external)
{
    if (!actual)
    {
        actual = std::make_unique<Actual>();
    }
    actual->buffer = external;
}

void RGResource<BufferDesc, FrameGraphBuffer>::realize(RenderGraphContext& ctx)
{
    if (_lifetime == ResourceLifetime::External)
    {
        if (ctx.compiled_)
        {
            std::cout << "[RG] Realize EXTERNAL FG Buffer \"" << _name << "\"\n";
        }
        return;
    }

    if (!ctx.vkCtx)
    {
        throw std::runtime_error("RenderGraphContext.vkCtx is null");
    }

    if (!actual)
    {
        actual = std::make_unique<Actual>();
    }

    if (!actual->buffer)
    {
        vkcore::BufferResource::Builder builder;
        builder.setSize(_desc.size)
               .setUsage(_desc.usage)
               .withVmaUsage(_desc.memoryUsage);

        auto unique_buffer = builder.buildUnique(*ctx.vkCtx);
        unique_buffer->setDebugName(_name);
        actual->buffer = std::shared_ptr<vkcore::BufferResource>(std::move(unique_buffer));
    }

    if (ctx.compiled_)
    {
        std::cout << "[RG] Realize FG Buffer \"" << _name << "\" (size=" << _desc.size << ")\n";
    }
}

void RGResource<BufferDesc, FrameGraphBuffer>::derealize(RenderGraphContext& ctx)
{
    (void)ctx;
    if (ctx.compiled_)
    {
        std::cout << "[RG] Derealize FG Buffer \"" << _name << "\"\n";
    }
}

}

