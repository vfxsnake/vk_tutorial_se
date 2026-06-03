#pragma once

#include <vulkan/vulkan_raii.hpp>
#include <string>

// forward declarations
class VulkanContext;
class ParticleDescriptorLayout;


class ParticleGraphicsPipeline
{
public:
    ParticleGraphicsPipeline(
        const VulkanContext& context, 
        const ParticleDescriptorLayout& particle_descriptor_layout,
        vk::Format color_format,
        vk::Format depth_format,
        vk::SampleCountFlagBits samples
    );

    // deleting copy operations
    ParticleGraphicsPipeline(const ParticleGraphicsPipeline&) = delete;
    ParticleGraphicsPipeline& operator =(const ParticleGraphicsPipeline&) = delete;

    void record(
        vk::CommandBuffer command_buffer,
        vk::Extent2D extent,
        vk::Image image,
        vk::ImageView color_view,
        vk::ImageView depth_view,
        vk::ImageView msaa_view,
        const vk::raii::DescriptorSet& compute_descriptor_set,
        vk::Buffer particle_buffer,
        uint32_t particle_count
    ) const;

private:
    void createPipelineLayout();
    void createPipeline(vk::Format color_format);

    auto createShaderModule(const std::string& spirv_path) const -> vk::raii::ShaderModule;
    
    static void transitionImageLayout(
        vk::CommandBuffer command_buffer,
        vk::Image image,
        vk::ImageLayout old_layout,
        vk::ImageLayout new_layout,
        vk::PipelineStageFlags2 source_stage_mask,
        vk::AccessFlags2 source_access_mask,
        vk::PipelineStageFlags2 destination_stage_mask,
        vk::AccessFlags2 destination_access_mask
    );

    const VulkanContext& context_;
    const ParticleDescriptorLayout& particleDescriptorLayout_;
    vk::raii::PipelineLayout layout_ = nullptr;
    vk::raii::Pipeline pipeline_ =  nullptr;
    vk::Format depthFormat_;
    vk::SampleCountFlagBits msaaSamples_;
};
