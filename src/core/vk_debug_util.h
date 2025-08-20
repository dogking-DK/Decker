#pragma once

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include <string>
#include <memory>
#include <mutex>
#include <fmt/format.h> // 用于格式化输出

#include "Macros.h"

namespace dk {
class DebugUtils
{
public:
    // 获取单例实例
    static DebugUtils& getInstance()
    {
        static DebugUtils instance;
        return instance;
    }

    // 初始化调试工具（必须在 Vulkan 实例创建后调用）
    void initialize(VkInstance instance)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (initialized_)
        {
            fmt::print("Warning: DebugUtils is already initialized\n");
            return;
        }

        vkSetDebugUtilsObjectNameEXT = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(
            vkGetInstanceProcAddr(instance, "vkSetDebugUtilsObjectNameEXT"));

        if (!vkSetDebugUtilsObjectNameEXT)
        {
            fmt::print("Warning: Failed to load vkSetDebugUtilsObjectNameEXT\n");
        }

        initialized_ = true;
    }

    // 设置对象名称
    void setDebugName(VkDevice device, VkObjectType type, uint64_t handle, const std::string& name) const
    {
        VkDebugUtilsObjectNameInfoEXT nameInfo{};
        nameInfo.sType        = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
        nameInfo.objectType   = type;
        nameInfo.objectHandle = handle;
        nameInfo.pObjectName  = name.c_str();

        VkResult result = vkSetDebugUtilsObjectNameEXT(device, &nameInfo);
        if (result != VK_SUCCESS)
        {
            fmt::print("Failed to set debug name for object: {}\n", string_VkResult(result));
        }
    }

private:
    // 私有构造函数，禁止外部创建实例
    DebugUtils() = default;

    // 禁止拷贝和赋值
    DebugUtils(const DebugUtils&)            = delete;
    DebugUtils& operator=(const DebugUtils&) = delete;

    PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectNameEXT = nullptr;
    bool                             initialized_                 = false;
    mutable std::mutex               mutex_; // 用于线程安全
};
} // dk
