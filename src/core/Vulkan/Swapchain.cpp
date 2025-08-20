#include "Swapchain.h"
#include "Context.h"

#include <fmt/base.h>

#include "Macros.h"

namespace dk::vkcore {
Swapchain::Swapchain(Swapchain& old_swapchain, const vk::Extent2D& extent) :
    Swapchain{
        *old_swapchain.context,
        old_swapchain.surface,
        old_swapchain._properties.present_mode,
        old_swapchain._present_mode_priority_list,
        old_swapchain._surface_format_priority_list,
        extent,
        old_swapchain._properties.image_count,
        old_swapchain._properties.pre_transform,
        old_swapchain._image_usage_flags,
        old_swapchain.get_handle()
    }
{
}

Swapchain::Swapchain(Swapchain& old_swapchain, const uint32_t image_count) :
    Swapchain{
        *old_swapchain.context,
        old_swapchain.surface,
        old_swapchain._properties.present_mode,
        old_swapchain._present_mode_priority_list,
        old_swapchain._surface_format_priority_list,
        old_swapchain._properties.extent,
        image_count,
        old_swapchain._properties.pre_transform,
        old_swapchain._image_usage_flags,
        old_swapchain.get_handle()
    }
{
}

Swapchain::Swapchain(Swapchain& old_swapchain, const std::set<vk::ImageUsageFlagBits>& image_usage_flags) :
    Swapchain{
        *old_swapchain.context,
        old_swapchain.surface,
        old_swapchain._properties.present_mode,
        old_swapchain._present_mode_priority_list,
        old_swapchain._surface_format_priority_list,
        old_swapchain._properties.extent,
        old_swapchain._properties.image_count,
        old_swapchain._properties.pre_transform,
        image_usage_flags,
        old_swapchain.get_handle()
    }
{
}

Swapchain::Swapchain(Swapchain&                            old_swapchain, const vk::Extent2D& extent,
                     const vk::SurfaceTransformFlagBitsKHR transform) :
    Swapchain{
        *old_swapchain.context,
        old_swapchain.surface,
        old_swapchain._properties.present_mode,
        old_swapchain._present_mode_priority_list,
        old_swapchain._surface_format_priority_list,
        extent,
        old_swapchain._properties.image_count,
        transform,
        old_swapchain._image_usage_flags,
        old_swapchain.get_handle()
    }
{
}

Swapchain::Swapchain(VulkanContext&                           context,
                     vk::SurfaceKHR                           surface,
                     const vk::PresentModeKHR                 present_mode,
                     const std::vector<vk::PresentModeKHR>&   present_mode_priority_list,
                     const std::vector<vk::SurfaceFormatKHR>& surface_format_priority_list,
                     const vk::Extent2D&                      extent,
                     const uint32_t                           image_count,
                     const vk::SurfaceTransformFlagBitsKHR    transform,
                     const std::set<vk::ImageUsageFlagBits>&  image_usage_flags,
                     vk::SwapchainKHR                         old_swapchain) :
    context{(&context)},
    surface{surface}
{
    vkb::SwapchainBuilder swapchainBuilder{context.getPhysicalDevice(), context.getDevice(), surface};

    uint32_t image_use_flag{0};
    for (auto flag : image_usage_flags)
    {
        image_use_flag |= static_cast<VkImageUsageFlagBits>(flag);
    }
    _image_usage_flags          = image_usage_flags;
    vkb::Swapchain vkbSwapchain = swapchainBuilder
                                  //.use_default_format_selection()
                                 .set_desired_format(VkSurfaceFormatKHR{
                                      .format = VK_FORMAT_B8G8R8A8_UNORM,
                                      .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
                                  })
                                  //use vsync present mode
                                 .set_desired_present_mode(static_cast<VkPresentModeKHR>(present_mode))
                                 .set_desired_extent(extent.width, extent.height)
                                 .add_image_usage_flags(image_use_flag)
                                 .set_required_min_image_count(image_count)
                                 .build()
                                 .value();

    _properties.extent       = vkbSwapchain.extent;
    _properties.present_mode = present_mode;
    _properties.image_count  = image_count;
    //store swapchain and its related images
    handle = vkbSwapchain.swapchain;
    for (auto swapchain_images = vkbSwapchain.get_images().value(); auto image : swapchain_images)
        images.emplace_back(image);
    for (auto swapchain_image_views = vkbSwapchain.get_image_views().value(); auto image_view : swapchain_image_views)
        image_views.emplace_back(image_view);
}

Swapchain::~Swapchain()
{
    clearSwapchain();
    clearSurface();
}

Swapchain::Swapchain(Swapchain&& other) :
    context{other.context},
    surface{std::exchange(other.surface, nullptr)},
    handle{std::exchange(other.handle, nullptr)},
    images{std::exchange(other.images, {})},
    _properties{std::exchange(other._properties, {})},
    _present_mode_priority_list{std::exchange(other._present_mode_priority_list, {})},
    _surface_format_priority_list{std::exchange(other._surface_format_priority_list, {})},
    _image_usage_flags{std::move(other._image_usage_flags)}
{
}

bool Swapchain::is_valid() const
{
    return !!handle;
}

const VulkanContext& Swapchain::get_device() const
{
    return *context;
}

vk::SwapchainKHR Swapchain::get_handle() const
{
    return handle;
}

std::pair<vk::Result, uint32_t> Swapchain::acquire_next_image(vk::Semaphore image_acquired_semaphore,
                                                              vk::Fence     fence) const
{
    vk::ResultValue<uint32_t> rv = context->getDevice().acquireNextImageKHR(
        handle, std::numeric_limits<uint64_t>::max(), image_acquired_semaphore, fence);
    return std::make_pair(rv.result, rv.value);
}

const vk::Extent2D& Swapchain::get_extent() const
{
    return _properties.extent;
}

vk::SurfaceTransformFlagBitsKHR Swapchain::get_transform() const
{
    return _properties.pre_transform;
}

vk::SurfaceKHR Swapchain::get_surface() const
{
    return surface;
}

vk::ImageUsageFlags Swapchain::get_usage() const
{
    return _properties.image_usage;
}

vk::PresentModeKHR Swapchain::get_present_mode() const
{
    return _properties.present_mode;
}

void Swapchain::clearSwapchain()
{
    if (handle)
    {
        context->getDevice().destroySwapchainKHR(handle);
        handle = nullptr;
    }

    // destroy swapchain resources
    for (auto& image_view : image_views)
    {
        if (image_view)
        {
            context->getDevice().destroyImageView(image_view);
            image_view = nullptr;
        }
        //vkDestroyImageView(_context->getDevice(), _swapchainImageViews[i], nullptr);
    }
}

void Swapchain::clearSurface()
{
    if (surface)
    {
        context->getInstance().destroySurfaceKHR(surface);
        //vkDestroySurfaceKHR(context->getInstance(), surface, nullptr);
        surface = nullptr;
    }
}
} // vkcore
