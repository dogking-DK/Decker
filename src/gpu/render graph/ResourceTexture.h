#pragma once
#include "Resource.h"
#include "Vulkan/Context.h"
#include "Vulkan/Texture.h"

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
        return texture ? texture->getImage() : VK_NULL_HANDLE;
    }

    vk::ImageView getVkImageView() const
    {
        return texture ? texture->getDefaultView() : VK_NULL_HANDLE;
    }

    vkcore::TextureResource*       getTexture() { return texture.get(); }
    const vkcore::TextureResource* getTexture() const { return texture.get(); }

private:
    std::shared_ptr<vkcore::TextureResource> texture{nullptr};
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
    // ★ 新增：把外部 TextureResource 塞进来
    void setExternal(const std::shared_ptr<vkcore::TextureResource>& texture);

    void realize(RenderGraphContext& ctx) override;
    void derealize(RenderGraphContext& ctx) override;

private:
    Desc                    _desc{};
    std::unique_ptr<Actual> actual; // 实际 image + default view 或外部句柄
};
} // namespace dk::rg
