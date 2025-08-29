#pragma once

#include "Resource.hpp"
#include "DescriptorSetLayout.h"
#include <vector>

namespace dk::vkcore {

    class PipelineLayout : public Resource<vk::PipelineLayout, vk::ObjectType::ePipelineLayout>
    {
    public:
        PipelineLayout(const PipelineLayout&) = delete;
        PipelineLayout& operator=(const PipelineLayout&) = delete;
        PipelineLayout(PipelineLayout&&) = default;
        PipelineLayout& operator=(PipelineLayout&&) = default;

        // 构造函数：接受 DescriptorSetLayout 列表和 PushConstantRange 列表
        PipelineLayout(VulkanContext* context,
            const std::vector<DescriptorSetLayout*>& set_layouts,
            const std::vector<vk::PushConstantRange>& push_constant_ranges = {})
            : Resource(context)
        {
            std::vector<vk::DescriptorSetLayout> layout_handles;
            layout_handles.reserve(set_layouts.size());
            for (const auto& layout : set_layouts)
            {
                layout_handles.push_back(layout->getHandle());
            }

            vk::PipelineLayoutCreateInfo layout_info;
            layout_info.setLayoutCount = static_cast<uint32_t>(layout_handles.size());
            layout_info.pSetLayouts = layout_handles.data();
            layout_info.pushConstantRangeCount = static_cast<uint32_t>(push_constant_ranges.size());
            layout_info.pPushConstantRanges = push_constant_ranges.data();

            _handle = _context->getDevice().createPipelineLayout(layout_info);
        }

        ~PipelineLayout() override
        {
            if (_handle)
            {
                _context->getDevice().destroyPipelineLayout(_handle);
            }
        }
    };

} // namespace dk::vkcore