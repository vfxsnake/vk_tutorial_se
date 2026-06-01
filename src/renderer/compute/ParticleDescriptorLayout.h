#pragma once

#include <vulkan/vulkan_raii.hpp>


// forward declarations
class VulkanContext;


class ParticleDescriptorLayout
{
public:
    ParticleDescriptorLayout(const VulkanContext& context);

    // removing copy constructures
    ParticleDescriptorLayout(const ParticleDescriptorLayout&) = delete;
    ParticleDescriptorLayout& operator =(const ParticleDescriptorLayout&) = delete;

    auto getLayout() const -> const vk::raii::DescriptorSetLayout&;

private:

    vk::raii::DescriptorSetLayout layout_ =  nullptr;
};
