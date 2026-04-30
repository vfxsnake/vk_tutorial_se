#include "FrameDescriptorLayout.h"
#include "core/VulkanContext.h"


FrameDescriptorLayout::FrameDescriptorLayout(const VulkanContext& context)
{
    // *Tutorial Reference: the tutorial the binding is defined with this initializer,
    // vk::DescriptorSetLayoutBinding binding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex, nullptr);
    // this should be the way using no constructors it is clear as we can see the parameter name.
    vk::DescriptorSetLayoutBinding binding{
        .binding = 0,
        .descriptorType = vk::DescriptorType::eUniformBuffer,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eVertex,
        .pImmutableSamplers = nullptr  
    };

    vk::DescriptorSetLayoutCreateInfo layout_info{
        .bindingCount = 1,
        .pBindings = &binding
    };

    layout_ = vk::raii::DescriptorSetLayout(context.getLogicalDevice(), layout_info);
}

const vk::raii::DescriptorSetLayout& FrameDescriptorLayout::getLayout() const
{
    return layout_;
}