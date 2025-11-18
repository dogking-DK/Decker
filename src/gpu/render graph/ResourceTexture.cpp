#include "ResourceTexture.h"

namespace dk {
void Resource<ImageDesc, FrameGraphImage>::realize(RenderGraphContext& ctx)
{
    if (lifetime == ResourceLifetime::External)
    {
        // 外部资源（比如 swapchain image）另说，这里先不管
        return;
    }
    if (!ctx.vkCtx)
    {
        throw std::runtime_error("RenderGraphContext.vkCtx is null");
    }
    if (!actual)
    {
        actual = std::make_unique<Actual>();

        // 1. 用你的 ImageBuilder 创建 ImageResource
        vkcore::ImageBuilder builder;
        builder.setFormat(desc.format)
               .setExtent(vk::Extent3D{desc.width, desc.height, desc.depth})
               .setUsage(desc.usage)
               .setTiling(desc.tiling)
               .setImageType(desc.type);
        // 如果你在 ImageBuilder 里扩展了 mipLevels/arrayLayers，顺便设置一下

        actual->image = std::make_unique<vkcore::ImageResource>(*ctx.vkCtx, builder);

        // 2. 创建一个默认的 ImageViewResource（比如 color attachment / sampled view）
        vkcore::ImageViewBuilder viewBuilder(*actual->image);
        viewBuilder.setFormat(desc.format)
                   .setAspectFlags(desc.aspectMask);
        actual->view = std::make_unique<vkcore::ImageViewResource>(*ctx.vkCtx, viewBuilder);
    }

    std::cout << "[RG] Realize FG Image \"" << name
        << "\" (" << desc.width << "x" << desc.height << ")\n";
}

void Resource<ImageDesc, FrameGraphImage>::derealize(RenderGraphContext& ctx)
{
    (void)ctx;
    if (lifetime == ResourceLifetime::Transient)
    {
        // FrameGraphImage 的析构顺序：先 view 再 image，
        // 会依次触发 ImageViewResource / ImageResource 的析构，
        // 自动 destroyImageView / vmaDestroyImage。
        actual.reset();
    }
    std::cout << "[RG] Derealize FG Image \"" << name << "\"\n";
}
}
