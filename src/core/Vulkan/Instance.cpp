#include "Instance.h"

#include <VkBootstrap.h>

#include "vk_debug_util.h"

namespace dk::vkcore {
    void Instance::initInstance()
    {
        vkb::InstanceBuilder builder;


        // make the vulkan instance, with basic debug features
        auto inst_ret = builder.set_app_name("Example Vulkan Application")
            .request_validation_layers(bUseValidationLayers)
            .use_default_debug_messenger()
            .enable_extension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)
            .require_api_version(1, 3, 0)
            .build();
        if (!inst_ret)
        {
            fmt::print(stderr, "Failed to create vulkan instance: {}\n", inst_ret.error().message());
        }

        vkb::Instance vkb_inst = inst_ret.value();

        // grab the instance
        _instance = vkb_inst.instance;
        _debug_messenger = vkb_inst.debug_messenger;

        DebugUtils::getInstance().initialize(_instance);
    }

    void Instance::initDevice()
    {

    }
}
