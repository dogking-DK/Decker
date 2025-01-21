#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <memory>
#include <mutex>
#include <fmt/format.h> // ���ڸ�ʽ�����

DECKER_START

class DebugUtils
{
public:
    // ��ȡ����ʵ��
    static DebugUtils& getInstance()
    {
        static DebugUtils instance;
        return instance;
    }

    // ��ʼ�����Թ��ߣ������� Vulkan ʵ����������ã�
    void initialize(VkInstance instance)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (initialized_)
        {
            fmt::print("Warning: DebugUtils is already initialized\n");
            return;
        }

        vkSetDebugUtilsObjectNameEXT = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(
            vkGetInstanceProcAddr(instance, "vkSetDebugUtilsObjectNameEXT")
        );

        if (!vkSetDebugUtilsObjectNameEXT)
        {
            fmt::print("Warning: Failed to load vkSetDebugUtilsObjectNameEXT\n");
        }

        initialized_ = true;
    }

    // ���ö�������
    void setDebugName(VkDevice device, VkObjectType type, uint64_t handle, const std::string& name) const
    {
        VkDebugUtilsObjectNameInfoEXT nameInfo{};
        nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
        nameInfo.objectType = type;
        nameInfo.objectHandle = handle;
        nameInfo.pObjectName = name.c_str();

        VkResult result = vkSetDebugUtilsObjectNameEXT(device, &nameInfo);
        if (result != VK_SUCCESS)
        {
            fmt::print("Failed to set debug name for object: {}\n", string_VkResult(result));
        }
    }

private:
    // ˽�й��캯������ֹ�ⲿ����ʵ��
    DebugUtils() = default;

    // ��ֹ�����͸�ֵ
    DebugUtils(const DebugUtils&) = delete;
    DebugUtils& operator=(const DebugUtils&) = delete;

    PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectNameEXT = nullptr;
    bool initialized_ = false;
    mutable std::mutex mutex_; // �����̰߳�ȫ
};

DECKER_END