#pragma once

#include <vulkan/vulkan_raii.hpp>


// forward declarations
class VulkanContext;
class ParticleDescriptorLayout;


class ComputePipeline
{
public:
    ComputePipeline(
        const VulkanContext& context,
        const ParticleDescriptorLayout& particle_descriptor_layout
    );

    // deleting copy operations.
    ComputePipeline(const ComputePipeline&) = delete;
    ComputePipeline& operator =(const ComputePipeline&) = delete;

    void record(
        vk::CommandBuffer command_buffer,
        const vk::raii::DescriptorSet& descriptor_set,
        uint32_t particle_count
    ) const;

private:
    void createPipelineLayout();
    void createPipeline();

    const VulkanContext& context_;
    const ParticleDescriptorLayout& particleDescriptorLayout_;
    vk::raii::PipelineLayout pipelineLayout_ = nullptr;
    vk::raii::Pipeline pipeline_ = nullptr;
};