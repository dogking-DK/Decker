#pragma once
#include "Resource.h"
#include "Vulkan/Context.h"
#include "Vulkan/Image.h"
#include "Vulkan/ImageView.h"
#include "Vulkan/ImageViewBuilder.h"


namespace dk {
struct ImageDesc
{
    uint32_t width  = 100;
    uint32_t height = 100;
    uint32_t depth  = 1;

    uint32_t mipLevels   = 1;
    uint32_t arrayLayers = 1;

    vk::Format              format  = vk::Format::eR16G16B16A16Sfloat;
    vk::ImageUsageFlags     usage   = vk::ImageUsageFlagBits::eSampled;
    vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1;
    vk::ImageTiling         tiling  = vk::ImageTiling::eOptimal;
    vk::ImageType           type    = vk::ImageType::e2D;

    vk::ImageAspectFlags aspectMask = vk::ImageAspectFlagBits::eColor;

    // 可选：内存策略，先简化只用 GPU_ONLY
    VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;
};

class FrameGraphImage
{
public:
    // 统一对外的取 VkImage / VkImageView 接口
    vk::Image getVkImage() const
    {
        return isExternal
                   ? externalImage
                   : (image ? image->getHandle() : VK_NULL_HANDLE);
    }

    vk::ImageView getVkImageView() const
    {
        return isExternal
                   ? externalImageView
                   : (view ? view->getHandle() : VK_NULL_HANDLE);
    }

    // 只有内部资源才会用到的 helper
    vkcore::ImageResource*           getImage() { return image.get(); }
    const vkcore::ImageResource*     getImage() const { return image.get(); }
    vkcore::ImageViewResource*       getView() { return view.get(); }
    const vkcore::ImageViewResource* getView() const { return view.get(); }

    // 方便构造一个“包了外部句柄”的 FG image
    static FrameGraphImage makeExternal(vk::Image image, vk::ImageView view)
    {
        FrameGraphImage r;
        r.isExternal        = true;
        r.externalImage     = image;
        r.externalImageView = view;
        return r;
    }

private:
    // 标记是否是外部资源
    bool isExternal = false;

    // ---- FG 自己管理的路径（Transient / Persistent）----
    // 用 unique_ptr 确保销毁顺序：先 view 后 image
    std::unique_ptr<vkcore::ImageResource>     image{ nullptr };
    std::unique_ptr<vkcore::ImageViewResource> view{ nullptr };  // 默认 view（可选）

    // ---- 外部资源路径（swapchain / drawImage 等）----
    vk::Image     externalImage = VK_NULL_HANDLE;
    vk::ImageView externalImageView = VK_NULL_HANDLE;
    friend class RGResource<ImageDesc, FrameGraphImage>;
};

template <>
class RGResource<ImageDesc, FrameGraphImage> : public RGResourceBase
{
public:
    using Desc   = ImageDesc;
    using Actual = FrameGraphImage;

    RGResource(const std::string& n, const Desc& d, ResourceLifetime life)
    {
        _name     = n;
        _desc     = d;
        _lifetime = life;
    }

    Actual*     get() const { return actual.get(); }
    const Desc& desc() const { return _desc; }
    // ★ 新增：把外部 VkImage / VkImageView 塞进来
    void setExternal(vk::Image image, vk::ImageView view);

    void realize(RenderGraphContext& ctx) override;
    void derealize(RenderGraphContext& ctx) override;

private:
    Desc                    _desc{};
    std::unique_ptr<Actual> actual; // 实际 image + default view 或外部句柄
};
} // namespace dk::rg
