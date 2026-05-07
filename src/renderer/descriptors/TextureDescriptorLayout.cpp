#include "TextureDescriptorLayout.h"
#include "core/VulkanContext.h"

TextureDescriptorLayout::TextureDescriptorLayout(const VulkanContext& context)
{
    vk::DescriptorSetLayoutBinding binding{
        .binding = 0,
        .descriptorType = vk::DescriptorType::eCombinedImageSampler,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eFragment,
        .pImmutableSamplers = nullptr
    };

    vk::DescriptorSetLayoutCreateInfo layout_info{
        .bindingCount = 1, 
        .pBindings = &binding
    };

    layout_ = vk::raii::DescriptorSetLayout(context.getLogicalDevice(), layout_info);
}

const vk::raii::DescriptorSetLayout& TextureDescriptorLayout::getLayout() const
{
    return layout_;
}