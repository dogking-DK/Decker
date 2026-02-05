#include "ResourceTexture.h"

namespace dk {
void RGResource<ImageDesc, FrameGraphImage>::setExternal(const std::shared_ptr<vkcore::TextureResource>& texture)
{
    if (!actual)
    {
        actual = std::make_unique<Actual>();
    }
    actual->texture = texture;
    glm::vec3 test = glm::vec3{1, 1, 1} * glm::vec3{3, 3, 3};
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

        vkcore::TextureResource::Builder builder;
        builder.setFormat(_desc.format)
               .setExtent(vk::Extent3D{_desc.width, _desc.height, _desc.depth})
               .setUsage(_desc.usage)
               .setTiling(_desc.tiling)
               .setImageType(_desc.type)
               .setSamples(_desc.samples)
               .setMipLevels(_desc.mipLevels)
               .setArrayLayers(_desc.arrayLayers)
               .withVmaUsage(_desc.memoryUsage);

        vkcore::TextureViewDesc view_desc{};
        view_desc.format = _desc.format;
        view_desc.aspectMask = _desc.aspectMask;
        builder.withDefaultView(view_desc);

        actual->texture = builder.buildShared(*ctx.vkCtx);
        if (auto* image = actual->texture ? actual->texture->getImageResource() : nullptr)
        {
            image->setDebugName(_name);
        }
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
