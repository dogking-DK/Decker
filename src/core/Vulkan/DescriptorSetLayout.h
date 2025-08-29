#pragma once

#include "Resource.hpp"
#include <vector>
#include <map>
#include <memory>

namespace dk::vkcore {

    class DescriptorSetLayout : public Resource<vk::DescriptorSetLayout, vk::ObjectType::eDescriptorSetLayout>
    {
    public:
        class Builder
        {
        public:
            Builder(VulkanContext* context) : _context(context) {}

            // 添加一个绑定
            Builder& addBinding(uint32_t binding, vk::DescriptorType type, vk::ShaderStageFlags stage_flags, uint32_t count = 1);

            // 为一个已添加的绑定设置标志 (用于 Bindless)
            Builder& setBindingFlags(uint32_t binding, vk::DescriptorBindingFlags flags);

            // 构建最终的 DescriptorSetLayout 对象
            std::unique_ptr<DescriptorSetLayout> build();

        private:
            VulkanContext* _context;
            std::vector<vk::DescriptorSetLayoutBinding> _bindings;
            std::map<uint32_t, vk::DescriptorBindingFlags> _binding_flags;
        };

        ~DescriptorSetLayout() override;

    private:
        // 构造函数设为私有，强制使用 Builder
        DescriptorSetLayout(VulkanContext* context, vk::DescriptorSetLayout layout)
            : Resource(context, layout) {
        }
    };

} // namespace dk::vkcore