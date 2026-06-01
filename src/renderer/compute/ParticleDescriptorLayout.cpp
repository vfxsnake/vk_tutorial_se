#include "ParticleDescriptorLayout.h"

#include "core/VulkanContext.h"

ParticleDescriptorLayout::ParticleDescriptorLayout(const VulkanContext& context)
{
    vk::DescriptorSetLayoutBinding layout_binding_0{
        .binding = 0,
        .descriptorType = vk::DescriptorType::eUniformBuffer,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eCompute,
        .pImmutableSamplers = nullptr
    };

    vk::DescriptorSetLayoutBinding layout_binding_1{
        .binding = 1,
        .descriptorType = vk::DescriptorType::eStorageBuffer,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eCompute,
        .pImmutableSamplers = nullptr
    };

    vk::DescriptorSetLayoutBinding layout_binding_2{
        .binding = 2,
        .descriptorType = vk::DescriptorType::eStorageBuffer,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eCompute,
        .pImmutableSamplers = nullptr
    };

    std::array<vk::DescriptorSetLayoutBinding, 3> layout_bindings{
        layout_binding_0, 
        layout_binding_1, 
        layout_binding_2
    };

    vk::DescriptorSetLayoutCreateInfo layout_info{
        .bindingCount = 3,
        .pBindings = layout_bindings.data()
    };

    layout_ = vk::raii::DescriptorSetLayout(context.getLogicalDevice(), layout_info);
}


const vk::raii::DescriptorSetLayout& ParticleDescriptorLayout::getLayout() const
{
    return layout_;
}