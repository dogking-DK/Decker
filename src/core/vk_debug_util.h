#pragma once
#include "vk_types.h"

//void setDebugName(VkDevice device, VkObjectType type, uint64_t handle, const std::string& name)
//{
//    assert(vkSetDebugUtilsObjectNameEXT);
//    VkDebugUtilsObjectNameInfoEXT nameInfo{};
//    nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
//    nameInfo.objectType = type;
//    nameInfo.objectHandle = handle;
//    nameInfo.pObjectName = name.c_str();
//    vkSetDebugUtilsObjectNameEXT(device, &nameInfo);
//}