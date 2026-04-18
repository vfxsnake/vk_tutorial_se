#pragma once

#include <vulkan/vulkan_raii.hpp>
#include <string>

// forward declaration
class VulkanContext;
class Mesh;


class GraphicsPipeline
{
public:
    GraphicsPipeline(const VulkanContext& context, vk::Format color_format);

    // Non-copyable (removing copyable constructors)
    GraphicsPipeline(const GraphicsPipeline&) = delete;
    GraphicsPipeline& operator =(const GraphicsPipeline&) = delete;

    void record(vk::CommandBuffer command_buffer, vk::Extent2D extent, vk::Image image,vk::ImageView image_view, const Mesh& mesh);
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
    vk::raii::PipelineLayout layout_ = nullptr;
    vk::raii::Pipeline pipeline_ = nullptr;
};