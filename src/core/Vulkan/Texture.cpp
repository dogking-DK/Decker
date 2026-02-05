#include "Texture.h"

namespace dk::vkcore {
namespace {
constexpr const char* kDefaultViewName = "default";
}

TextureResource::Builder::Builder() = default;

TextureResource::Builder& TextureResource::Builder::setFormat(vk::Format format)
{
    _image_builder.setFormat(format);
    return *this;
}

TextureResource::Builder& TextureResource::Builder::setUsage(vk::ImageUsageFlags usage)
{
    _image_builder.setUsage(usage);
    return *this;
}

TextureResource::Builder& TextureResource::Builder::setExtent(vk::Extent3D extent)
{
    _image_builder.setExtent(extent);
    return *this;
}

TextureResource::Builder& TextureResource::Builder::setTiling(vk::ImageTiling tiling)
{
    _image_builder.setTiling(tiling);
    return *this;
}

TextureResource::Builder& TextureResource::Builder::setImageType(vk::ImageType type)
{
    _image_builder.setImageType(type);
    return *this;
}

TextureResource::Builder& TextureResource::Builder::setInitialLayout(vk::ImageLayout layout)
{
    _image_builder.setInitialLayout(layout);
    return *this;
}

TextureResource::Builder& TextureResource::Builder::setSamples(vk::SampleCountFlagBits samples)
{
    _image_builder.getCreateInfo().samples = samples;
    return *this;
}

TextureResource::Builder& TextureResource::Builder::setMipLevels(uint32_t mip_levels)
{
    _image_builder.getCreateInfo().mipLevels = mip_levels;
    return *this;
}

TextureResource::Builder& TextureResource::Builder::setArrayLayers(uint32_t layers)
{
    _image_builder.getCreateInfo().arrayLayers = layers;
    return *this;
}

TextureResource::Builder& TextureResource::Builder::withVmaFlags(VmaAllocationCreateFlags flags)
{
    _image_builder.withVmaFlags(flags);
    return *this;
}

TextureResource::Builder& TextureResource::Builder::withVmaPool(VmaPool pool)
{
    _image_builder.withVmaPool(pool);
    return *this;
}

TextureResource::Builder& TextureResource::Builder::withVmaPreferredFlags(vk::MemoryPropertyFlags flags)
{
    _image_builder.withVmaPreferredFlags(flags);
    return *this;
}

TextureResource::Builder& TextureResource::Builder::withVmaRequiredFlags(vk::MemoryPropertyFlags flags)
{
    _image_builder.withVmaRequiredFlags(flags);
    return *this;
}

TextureResource::Builder& TextureResource::Builder::withVmaUsage(VmaMemoryUsage usage)
{
    _image_builder.withVmaUsage(usage);
    return *this;
}

TextureResource::Builder& TextureResource::Builder::withDefaultView(const TextureViewDesc& desc)
{
    _default_view_desc = desc;
    _create_default_view = true;
    return *this;
}

TextureResource::Builder& TextureResource::Builder::disableDefaultView()
{
    _create_default_view = false;
    return *this;
}

std::unique_ptr<TextureResource> TextureResource::Builder::buildUnique(VulkanContext& context)
{
    return std::make_unique<TextureResource>(context, *this);
}

std::shared_ptr<TextureResource> TextureResource::Builder::buildShared(VulkanContext& context)
{
    return std::make_shared<TextureResource>(context, *this);
}

TextureResource::TextureResource(VulkanContext& context, Builder& builder)
    : _context(&context)
{
    _image = builder._image_builder.buildUnique(context);
    _format = builder._image_builder.getCreateInfo().format;
    _extent = builder._image_builder.getCreateInfo().extent;

    if (builder._create_default_view)
    {
        TextureViewDesc desc = builder._default_view_desc;
        if (desc.format == vk::Format::eUndefined)
        {
            desc.format = _format;
        }
        createView(kDefaultViewName, desc);
        _default_view_name = kDefaultViewName;
    }
}

std::shared_ptr<TextureResource> TextureResource::wrapExternal(
    vk::Image      image,
    vk::ImageView  default_view,
    vk::Format     format,
    vk::Extent3D   extent)
{
    auto tex = std::shared_ptr<TextureResource>(new TextureResource());
    tex->setExternal(image, default_view, format, extent);
    return tex;
}

void TextureResource::setExternal(vk::Image image, vk::ImageView default_view, vk::Format format, vk::Extent3D extent)
{
    _external = true;
    _external_image = image;
    _format = format;
    _extent = extent;
    _image.reset();
    _views.clear();
    _external_views.clear();
    if (default_view)
    {
        _external_views[kDefaultViewName] = default_view;
        _default_view_name = kDefaultViewName;
    }
}

vk::Image TextureResource::getImage() const
{
    if (_external)
    {
        return _external_image;
    }
    return _image ? _image->getHandle() : vk::Image{};
}

vk::ImageView TextureResource::getDefaultView() const
{
    if (_default_view_name.empty())
    {
        return vk::ImageView{};
    }

    if (_external)
    {
        auto it = _external_views.find(_default_view_name);
        return it != _external_views.end() ? it->second : vk::ImageView{};
    }

    auto it = _views.find(_default_view_name);
    return it != _views.end() ? it->second->getHandle() : vk::ImageView{};
}

vk::ImageView TextureResource::getView(const std::string& name) const
{
    if (_external)
    {
        auto it = _external_views.find(name);
        return it != _external_views.end() ? it->second : vk::ImageView{};
    }

    auto it = _views.find(name);
    return it != _views.end() ? it->second->getHandle() : vk::ImageView{};
}

ImageViewResource* TextureResource::getViewResource(const std::string& name)
{
    auto it = _views.find(name);
    return it != _views.end() ? it->second.get() : nullptr;
}

const ImageViewResource* TextureResource::getViewResource(const std::string& name) const
{
    auto it = _views.find(name);
    return it != _views.end() ? it->second.get() : nullptr;
}

ImageViewResource* TextureResource::createView(const std::string& name, const TextureViewDesc& desc)
{
    if (_external)
    {
        return nullptr;
    }
    if (!_image)
    {
        return nullptr;
    }

    auto view = std::make_unique<ImageViewResource>(
        *_context,
        *_image,
        desc.viewType,
        desc.format == vk::Format::eUndefined ? _format : desc.format,
        desc.aspectMask,
        desc.baseMipLevel,
        desc.levelCount,
        desc.baseLayer,
        desc.layerCount);

    auto* ptr = view.get();
    _views[name] = std::move(view);
    if (_default_view_name.empty())
    {
        _default_view_name = name;
    }
    return ptr;
}

} // namespace dk::vkcore
