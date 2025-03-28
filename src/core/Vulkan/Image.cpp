#include "Image.h"

#include "ImageBuilder.h"

namespace dk::vkcore {
ImageResource::ImageResource(VulkanContext& context, ImageBuilder& builder)
{
    VkResult result = vmaCreateImage(context.getVmaAllocator(),
                                     reinterpret_cast<const VkImageCreateInfo*>(&builder.getCreateInfo()),
                                     &builder.getAllocationCreateInfo(),
                                     reinterpret_cast<VkImage*>(&_handle),
                                     &_allocation,
                                     nullptr);
    
    _allocation_create_info = builder.getAllocationCreateInfo(); // �������Ĵ�����Ϣ

    if (result != VK_SUCCESS)
    {
        fmt::print("image create fail\n");
    }
}
}
