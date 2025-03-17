#pragma once
#define bUseValidationLayers true
#include <vulkan/vulkan_core.h>

namespace dk::vkcore {
    class Instance
    {
    public:
        Instance() = default;
        ~Instance() = default;

        void initInstance();

        [[nodiscard]]VkInstance getInstance() const { return _instance; }
        [[nodiscard]]VkDebugUtilsMessengerEXT getDebugMessenger() const { return _debug_messenger; }

    private:
        VkInstance _instance;
        VkDebugUtilsMessengerEXT _debug_messenger;


    }
}
