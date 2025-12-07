#include "ResourceTexture.h"

namespace dk {
void RGResource<ImageDesc, FrameGraphImage>::setExternal(vk::Image image, vk::ImageView view)
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

void RGResource<ImageDesc, FrameGraphImage>::realize(RenderGraphContext& ctx)
{
    // ★ 外部资源：FG 不创建 / 管理，只是个占位 + debug 打印
    if (_lifetime == ResourceLifetime::External)
    {
        if (ctx.compiled_)
        {
            std::cout << "[RG] Realize EXTERNAL FG Image \"" << _name << "\"\n";
        }

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
        builder.setFormat(_desc.format)
               .setExtent(vk::Extent3D{_desc.width, _desc.height, _desc.depth})
               .setUsage(_desc.usage)
               .setTiling(_desc.tiling)
               .setImageType(_desc.type)
               .withVmaUsage(VMA_MEMORY_USAGE_AUTO);

        actual->image = builder.buildUnique(*ctx.vkCtx);
        actual->image->setDebugName(_name);
        // 2. 创建一个默认的 ImageViewResource（比如 color attachment / sampled view）
        vkcore::ImageViewBuilder viewBuilder(*actual->image);
        viewBuilder.setFormat(_desc.format)
                   .setAspectFlags(_desc.aspectMask);
        actual->view = std::make_unique<vkcore::ImageViewResource>(*ctx.vkCtx, viewBuilder);
    }
    if (ctx.compiled_)
    {
        std::cout << "[RG] Realize FG Image \"" << _name
            << "\" (" << _desc.width << "x" << _desc.height << ")\n";
    }
}

void RGResource<ImageDesc, FrameGraphImage>::derealize(RenderGraphContext& ctx)
{
    (void)ctx;
    if (_lifetime == ResourceLifetime::Transient)
    {
        //// FrameGraphImage 的析构顺序：先 view 再 image，
        //// 会依次触发 ImageViewResource / ImageResource 的析构，
        //// 自动 destroyImageView / vmaDestroyImage。
        ////actual.reset();
        //ctx.frame_data->_deletionQueue.push_function([img = std::move(actual)]() mutable
        //{
        //    // 这里才真正 reset，调用 ~ImageResource -> vkDestroyImage/vmaFree
        //    img.reset();
        //});
    }
    if (ctx.compiled_)
    {
        std::cout << "[RG] Derealize FG Image \"" << _name << "\"\n";
    }
}
}
