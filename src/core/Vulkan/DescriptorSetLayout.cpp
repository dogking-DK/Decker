#include "DescriptorSetLayout.h"
#include <stdexcept>

namespace dk::vkcore {

    DescriptorSetLayout::Builder& DescriptorSetLayout::Builder::addBinding(uint32_t binding, vk::DescriptorType type,
        vk::ShaderStageFlags stage_flags, uint32_t count)
    {
        for (const auto& b : _bindings) {
            if (b.binding == binding) {
                throw std::runtime_error("Binding " + std::to_string(binding) + " is already in use.");
            }
        }
        _bindings.push_back({ binding, type, count, stage_flags, nullptr });
        return *this;
    }

    DescriptorSetLayout::Builder& DescriptorSetLayout::Builder::setBindingFlags(uint32_t binding, vk::DescriptorBindingFlags flags)
    {
        _binding_flags[binding] = flags;
        return *this;
    }

    std::unique_ptr<DescriptorSetLayout> DescriptorSetLayout::Builder::build()
    {
        vk::DescriptorSetLayoutCreateInfo layout_info;
        layout_info.bindingCount = static_cast<uint32_t>(_bindings.size());
        layout_info.pBindings = _bindings.data();

        // 为 Bindless 链接扩展信息
        std::vector<vk::DescriptorBindingFlags> flags_list(_bindings.size(), {});
        if (!_binding_flags.empty())
        {
            for (size_t i = 0; i < _bindings.size(); ++i)
            {
                auto it = _binding_flags.find(_bindings[i].binding);
                if (it != _binding_flags.end())
                {
                    flags_list[i] = it->second;
                }
            }
            vk::DescriptorSetLayoutBindingFlagsCreateInfo extended_info;
            extended_info.bindingCount = static_cast<uint32_t>(flags_list.size());
            extended_info.pBindingFlags = flags_list.data();
            layout_info.pNext = &extended_info;
        }

        vk::DescriptorSetLayout layout = _context->getDevice().createDescriptorSetLayout(layout_info);
        // 使用友元或私有构造函数技巧来封装
        return std::unique_ptr<DescriptorSetLayout>(new DescriptorSetLayout(_context, layout));
    }

    DescriptorSetLayout::~DescriptorSetLayout()
    {
        if (hasDevice() && hasHandle())
        {
            _context->getDevice().destroyDescriptorSetLayout(_handle);
        }
    }
} // namespace dk::vkcore