#pragma once
#include "Resource.h"
#include "Vulkan/Context.h"
#include "Vulkan/Image.h"
#include "Vulkan/ImageBuilder.h"
#include "Vulkan/ImageView.h"
#include "Vulkan/ImageViewBuilder.h"


namespace dk {
struct ImageDesc
{
    uint32_t width  = 0;
    uint32_t height = 0;
    uint32_t depth  = 1;

    uint32_t mipLevels   = 1;
    uint32_t arrayLayers = 1;

    vk::Format              format  = vk::Format::eUndefined;
    vk::ImageUsageFlags     usage   = {};
    vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1;
    vk::ImageTiling         tiling  = vk::ImageTiling::eOptimal;
    vk::ImageType           type    = vk::ImageType::e2D;

    vk::ImageAspectFlags aspectMask = vk::ImageAspectFlagBits::eColor;

    // 可选：内存策略，先简化只用 GPU_ONLY
    VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;
};

struct FrameGraphImage
{
    // 用 unique_ptr 确保销毁顺序：先 view 后 image
    std::unique_ptr<vkcore::ImageResource>     image;
    std::unique_ptr<vkcore::ImageViewResource> view;  // 默认 view（可选）

    vk::Image     getVkImage() const { return image ? image->getHandle() : VK_NULL_HANDLE; }
    vk::ImageView getVkImageView() const { return view ? view->getHandle() : VK_NULL_HANDLE; }

    vkcore::ImageResource*           getImage() { return image.get(); }
    const vkcore::ImageResource*     getImage() const { return image.get(); }
    vkcore::ImageViewResource*       getView() { return view.get(); }
    const vkcore::ImageViewResource* getView() const { return view.get(); }
};



template <>
struct Resource<ImageDesc, FrameGraphImage> : ResourceBase
{
    using Desc   = ImageDesc;
    using Actual = FrameGraphImage;

    Desc                    desc{};
    std::unique_ptr<Actual> actual; // 实际 image + default view

    Resource(const std::string& n, const Desc& d, ResourceLifetime life)
    {
        name     = n;
        desc     = d;
        lifetime = life;
    }

    Actual* get() { return actual.get(); }

    void realize(RenderGraphContext& ctx) override;

    void derealize(RenderGraphContext& ctx) override;
};
} // namespace dk::rg
