#pragma once

#include <vulkan/vulkan_raii.hpp>
#include <string>
#include <span>

// forward declaration
class VulkanContext;
class FrameDescriptorLayout;
class Mesh;
class TextureDescriptorLayout;


class GraphicsPipeline
{
public:
    GraphicsPipeline(
        const VulkanContext& context, 
        const FrameDescriptorLayout& frame_descriptor_layout,
        const TextureDescriptorLayout& texture_descriptor_layout,
        vk::Format color_format,
        vk::Format depth_format,
        vk::SampleCountFlagBits msaa_samples
    );

    // Non-copyable (removing copyable constructors)
    GraphicsPipeline(const GraphicsPipeline&) = delete;
    GraphicsPipeline& operator =(const GraphicsPipeline&) = delete;

    void record(
        vk::CommandBuffer command_buffer, 
        std::span<const vk::DescriptorSet> descriptor_sets,
        const vk::raii::DescriptorSet& texture_descriptor_set,
        vk::Extent2D extent, 
        vk::Image image, 
        vk::ImageView image_view,
        vk::ImageView msaa_color_image_view,
        std::span<const Mesh*> meshes,
        vk::ImageView depth_image_view
    );

    auto getPipeline() const -> const vk::raii::Pipeline&;

private:
    void createPipelineLayout();
    void createPipeline(vk::Format color_format);

    auto createShaderModule(const std::string& spirv_path) const -> vk::raii::ShaderModule;

    // image layout transition helper (synchronization2)
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

    // private member variables
    const VulkanContext& context_;
    const FrameDescriptorLayout& frameDescriptorLayout_;
    const TextureDescriptorLayout& textureDescriptorLayout_;
    vk::raii::PipelineLayout layout_ = nullptr;
    vk::raii::Pipeline pipeline_ = nullptr;
    vk::Format depthFormat_;
    vk::SampleCountFlagBits msaaSamples_;
};