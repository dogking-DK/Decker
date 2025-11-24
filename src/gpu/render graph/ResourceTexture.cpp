#include "ResourceTexture.h"

namespace dk {
void Resource<ImageDesc, FrameGraphImage>::setExternal(vk::Image image, vk::ImageView view)
{
    if (!actual)
    {
        actual = std::make_unique<Actual>();
    }

    actual->isExternal        = true;
    actual->externalImage     = image;
    actual->externalImageView = view;

    // 确保不会误用内部路径
    actual->image.reset();
    actual->view.reset();
}

void Resource<ImageDesc, FrameGraphImage>::realize(RenderGraphContext& ctx)
{
    // ★ 外部资源：FG 不创建 / 管理，只是个占位 + debug 打印
    if (lifetime == ResourceLifetime::External)
    {
        std::cout << "[RG] Realize EXTERNAL FG Image \"" << name << "\"\n";
        // 这里假定 setExternal() 已经在 execute 之前被调用
        return;
    }

    // 以下是原来的内部资源路径
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
               .setImageType(desc.type)
               .withVmaUsage(VMA_MEMORY_USAGE_AUTO);

        actual->image = builder.buildUnique(*ctx.vkCtx);

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
