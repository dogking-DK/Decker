#pragma once

#include <SDL_video.h>
#include <set>
#include <vulkan/vulkan.hpp>

#include "Macros.h"

namespace dk::vkcore {
class VulkanContext;

struct HPPSwapchainProperties
{
    vk::SwapchainKHR                old_swapchain;
    uint32_t                        image_count{3};
    vk::Extent2D                    extent;
    vk::SurfaceFormatKHR            surface_format{ static_cast<vk::Format>(VK_FORMAT_B8G8R8A8_UNORM) };
    uint32_t                        array_layers;
    vk::ImageUsageFlags             image_usage;
    vk::SurfaceTransformFlagBitsKHR pre_transform;
    vk::CompositeAlphaFlagBitsKHR   composite_alpha;
    vk::PresentModeKHR              present_mode;
};

class Swapchain
{
public:
    /**
     * @brief Constructor to create a swapchain by changing the extent
     *        only and preserving the configuration from the old swapchain.
     */
    Swapchain(Swapchain& old_swapchain, const vk::Extent2D& extent);

    /**
     * @brief Constructor to create a swapchain by changing the image count
     *        only and preserving the configuration from the old swapchain.
     */
    Swapchain(Swapchain& old_swapchain, uint32_t image_count);

    /**
     * @brief Constructor to create a swapchain by changing the image usage
     *        only and preserving the configuration from the old swapchain.
     */
    Swapchain(Swapchain& old_swapchain, const std::set<vk::ImageUsageFlagBits>& image_usage_flags);

    /**
     * @brief Constructor to create a swapchain by changing the extent
     *        and transform only and preserving the configuration from the old swapchain.
     */
    Swapchain(Swapchain& swapchain, const vk::Extent2D& extent, vk::SurfaceTransformFlagBitsKHR transform);

    /**
     * @brief Constructor to create a swapchain.
     */
    Swapchain(VulkanContext&                         context,
                 vk::SurfaceKHR                         surface,
                 vk::PresentModeKHR                     present_mode,
                 const std::vector<vk::PresentModeKHR>& present_mode_priority_list = {
                     vk::PresentModeKHR::eFifo, vk::PresentModeKHR::eMailbox
                 },
                 const std::vector<vk::SurfaceFormatKHR>& surface_format_priority_list = {
                     {vk::Format::eR8G8B8A8Srgb, vk::ColorSpaceKHR::eSrgbNonlinear},
                     {vk::Format::eB8G8R8A8Srgb, vk::ColorSpaceKHR::eSrgbNonlinear}
                 },
                 const vk::Extent2D&                     extent            = {},
                 uint32_t                                image_count       = 3,
                 vk::SurfaceTransformFlagBitsKHR         transform         = vk::SurfaceTransformFlagBitsKHR::eIdentity,
                 const std::set<vk::ImageUsageFlagBits>& image_usage_flags = {
                     vk::ImageUsageFlagBits::eColorAttachment, vk::ImageUsageFlagBits::eTransferDst
                 },
                 vk::SwapchainKHR old_swapchain = nullptr);

    Swapchain(const Swapchain&) = delete;

    Swapchain(Swapchain&& other);

    ~Swapchain();

    Swapchain& operator=(const Swapchain&) = delete;

    Swapchain& operator=(Swapchain&&) = delete;

    bool is_valid() const;

    const VulkanContext& get_device() const;

    vk::SwapchainKHR get_handle() const;

    std::pair<vk::Result, uint32_t> acquire_next_image(vk::Semaphore image_acquired_semaphore,
                                                       vk::Fence     fence = nullptr) const;

    const vk::Extent2D& get_extent() const;

    vk::Format get_format() const { return properties.surface_format.format; }

    const std::vector<vk::Image>& get_images() const { return images; }

    const std::vector<vk::ImageView>& get_image_views() const { return image_views; }


    vk::SurfaceTransformFlagBitsKHR get_transform() const;

    vk::SurfaceKHR get_surface() const;

    vk::ImageUsageFlags get_usage() const;

    vk::PresentModeKHR get_present_mode() const;

    void clearSwapchain();
    void clearSurface();
private:
    VulkanContext* context;

    vk::SurfaceKHR surface;

    vk::SwapchainKHR handle;

    std::vector<vk::Image> images;
    std::vector<vk::ImageView> image_views;

    HPPSwapchainProperties properties;

    // A list of present modes in order of priority (vector[0] has high priority, vector[size-1] has low priority)
    std::vector<vk::PresentModeKHR> present_mode_priority_list;

    // A list of surface formats in order of priority (vector[0] has high priority, vector[size-1] has low priority)
    std::vector<vk::SurfaceFormatKHR> surface_format_priority_list;

    std::set<vk::ImageUsageFlagBits> image_usage_flags;
};
} // dk::vkcore
