#include "Image.h"

#include "ImageBuilder.h"

namespace dk::vkcore {
ImageResource::ImageResource(VulkanContext& context, ImageBuilder& builder)
    : Resource(&context, nullptr)
{
    VkResult result = vmaCreateImage(context.getVmaAllocator(),
                                     reinterpret_cast<const VkImageCreateInfo*>(&builder.getCreateInfo()),
                                     &builder.getAllocationCreateInfo(),
                                     reinterpret_cast<VkImage*>(&_handle),
                                     &_allocation,
                                     nullptr);
    
    _allocation_create_info = builder.getAllocationCreateInfo(); // 储存分配的创建信息

    if (result != VK_SUCCESS)
    {
        fmt::print("image create fail\n");
    }
}
}
