#pragma once

#include <string>
#include <unordered_map>
#include <memory>

#include "Image.h"
#include "ImageView.h"

namespace dk::vkcore {
struct TextureViewDesc
{
    vk::ImageViewType    viewType      = vk::ImageViewType::e2D;
    vk::Format           format        = vk::Format::eUndefined;
    vk::ImageAspectFlags aspectMask    = vk::ImageAspectFlagBits::eColor;
    uint32_t             baseMipLevel  = 0;
    uint32_t             levelCount    = 1;
    uint32_t             baseLayer     = 0;
    uint32_t             layerCount    = 1;
};

class TextureResource
{
public:
    class Builder
    {
    public:
        Builder();

        Builder& setFormat(vk::Format format);
        Builder& setUsage(vk::ImageUsageFlags usage);
        Builder& setExtent(vk::Extent3D extent);
        Builder& setTiling(vk::ImageTiling tiling);
        Builder& setImageType(vk::ImageType type);
        Builder& setInitialLayout(vk::ImageLayout layout);
        Builder& setSamples(vk::SampleCountFlagBits samples);
        Builder& setMipLevels(uint32_t mip_levels);
        Builder& setArrayLayers(uint32_t layers);

        Builder& withVmaFlags(VmaAllocationCreateFlags flags);
        Builder& withVmaPool(VmaPool pool);
        Builder& withVmaPreferredFlags(vk::MemoryPropertyFlags flags);
        Builder& withVmaRequiredFlags(vk::MemoryPropertyFlags flags);
        Builder& withVmaUsage(VmaMemoryUsage usage);

        Builder& withDefaultView(const TextureViewDesc& desc);
        Builder& disableDefaultView();

        std::unique_ptr<TextureResource> buildUnique(VulkanContext& context);
        std::shared_ptr<TextureResource> buildShared(VulkanContext& context);

        const ImageResource::Builder& getImageBuilder() const { return _image_builder; }
        ImageResource::Builder&       getImageBuilder() { return _image_builder; }

    private:
        ImageResource::Builder _image_builder{};
        TextureViewDesc        _default_view_desc{};
        bool                   _create_default_view{true};

        friend class TextureResource;
    };

    TextureResource(VulkanContext& context, Builder& builder);
    ~TextureResource() = default;

    static std::shared_ptr<TextureResource> wrapExternal(
        vk::Image      image,
        vk::ImageView  default_view,
        vk::Format     format,
        vk::Extent3D   extent = vk::Extent3D{0, 0, 1});

    void setExternal(vk::Image image, vk::ImageView default_view, vk::Format format, vk::Extent3D extent);

    bool isExternal() const { return _external; }

    vk::Image     getImage() const;
    vk::ImageView getDefaultView() const;
    vk::ImageView getView(const std::string& name) const;

    ImageResource*       getImageResource() { return _image.get(); }
    const ImageResource* getImageResource() const { return _image.get(); }

    ImageViewResource*       getViewResource(const std::string& name);
    const ImageViewResource* getViewResource(const std::string& name) const;

    ImageViewResource* createView(const std::string& name, const TextureViewDesc& desc);

    vk::Format   getFormat() const { return _format; }
    vk::Extent3D getExtent() const { return _extent; }

private:
    TextureResource() = default;

    VulkanContext* _context{nullptr};
    bool           _external{false};
    vk::Image      _external_image{VK_NULL_HANDLE};

    vk::Format   _format{vk::Format::eUndefined};
    vk::Extent3D _extent{0, 0, 1};

    std::unique_ptr<ImageResource> _image;
    std::unordered_map<std::string, std::unique_ptr<ImageViewResource>> _views;
    std::unordered_map<std::string, vk::ImageView> _external_views;
    std::string _default_view_name;
};
} // namespace dk::vkcore
