#pragma once

#include <vulkan/vulkan_raii.hpp>

// forward declaration
class VulkanContext;

class TextureDescriptorLayout
{
public:
    TextureDescriptorLayout(const VulkanContext& context);

    // removing copy constructors
    TextureDescriptorLayout(const TextureDescriptorLayout&) = delete;
    TextureDescriptorLayout& operator =(const TextureDescriptorLayout&) = delete;

    auto getLayout() const -> const vk::raii::DescriptorSetLayout&;

private:
    vk::raii::DescriptorSetLayout layout_ = nullptr;
};