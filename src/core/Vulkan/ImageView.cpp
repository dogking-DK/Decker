#include "ImageView.h"

#include "ImageViewBuilder.h"

namespace dk::vkcore {
ImageViewResource::ImageViewResource(VulkanContext& context, ImageViewBuilder& builder)
    : Resource(&context, nullptr), _image(&builder.getImage())
{

}
}
