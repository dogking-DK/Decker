#pragma once
#include <vk_types.h>
#include <string>
#include <vector>

namespace dk::core {
/**
 * @brief An interface class, declaring the behaviour of a Window
 */
class Window
{
public:
    struct Extent
    {
        uint32_t width;
        uint32_t height;
    };

    enum class mode : uint8_t
    {
        Headless,
        Fullscreen,
        FullscreenBorderless,
        FullscreenStretch,
        Default
    };

    enum class vsync : uint8_t
    {
        OFF,
        ON,
        Default
    };

    struct Properties
    {
        std::string title;
        mode        mode      = mode::Default;
        bool        resizable = true;
        vsync       vsync     = vsync::Default;
        Extent      extent{.width = 1280, .height = 720};
    };

    Window(const Properties& properties);

    virtual ~Window() = default;

    virtual VkSurfaceKHR create_surface(vk::Instance& instance) = 0;

    /**
     * @brief Gets a handle from the platform's Vulkan surface
     * @param instance A Vulkan instance
     * @param physical_device A Vulkan PhysicalDevice
     * @returns A VkSurfaceKHR handle, for use by the application
     */
    virtual VkSurfaceKHR create_surface(VkInstance instance, VkPhysicalDevice physical_device) = 0;

    /**
     * @brief Checks if the window should be closed
     */
    virtual bool should_close() = 0;

    /**
     * @brief Requests to close the window
     */
    virtual void close() = 0;

    /**
     * @return The dot-per-inch scale factor
     */
    virtual float get_dpi_factor() const = 0;

    /**
     * @return The scale factor for systems with heterogeneous window and pixel coordinates
     */
    virtual float get_content_scale_factor() const;

    /**
     * @brief Attempt to resize the window - not guaranteed to change
     *
     * @param extent The preferred window extent
     * @return Extent The new window extent
     */
    Extent resize(const Extent& extent);

    /**
     * @brief Get the display present info for the window if needed
     *
     * @param info Filled in when the method returns true
     * @param src_width The width of the surface being presented
     * @param src_height The height of the surface being presented
     * @return true if the present info was filled in and should be used
     * @return false if the extra present info should not be used. info is left untouched.
     */
    virtual bool get_display_present_info(VkDisplayPresentInfoKHR* info,
                                          uint32_t                 src_width, uint32_t src_height) const;

    virtual std::vector<const char*> get_required_surface_extensions() const = 0;

    const Extent& get_extent() const;

    mode get_window_mode() const;

    const Properties& get_properties() const
    {
        return _properties;
    }

protected:
    Properties _properties;
};
}
