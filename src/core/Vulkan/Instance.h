#pragma once
#define bUseValidationLayers true
#include <VkBootstrap.h>
#include <vulkan/vulkan_core.h>

namespace dk {
    class Instance
    {
    public:
        Instance() = default;
        ~Instance() = default;

        void initInstance();
        void initDevice();

        [[nodiscard]] VkInstance getInstance() const { return _instance; }
        [[nodiscard]] VkDebugUtilsMessengerEXT getDebugMessenger() const { return _debug_messenger; }

    private:
        VkInstance _instance;
        VkDebugUtilsMessengerEXT _debug_messenger;
        vkb::Instance _vkb_inst;

    };
}
