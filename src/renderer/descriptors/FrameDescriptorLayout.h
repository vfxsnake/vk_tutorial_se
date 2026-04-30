#pragma once

#include <vulkan/vulkan_raii.hpp>

// forward declaration:
class VulkanContext;


class FrameDescriptorLayout
{
public:
    FrameDescriptorLayout(const VulkanContext& context);
    
    // removing copy constructors
    FrameDescriptorLayout(const FrameDescriptorLayout&) = delete;
    FrameDescriptorLayout& operator =(const FrameDescriptorLayout&) = delete;

    auto getLayout() const -> const vk::raii::DescriptorSetLayout&;

private:
    vk::raii::DescriptorSetLayout layout_ = nullptr;

};
