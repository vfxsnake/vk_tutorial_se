#include "GraphicsPipeline.h"
#include "core/VulkanContext.h"
#include "utils/FileUtils.h"
#include "renderer/buffers/Vertex.h"
#include "renderer/buffers/Mesh.h"

#include <stdexcept>
#include <array>


GraphicsPipeline::GraphicsPipeline(const VulkanContext& context, vk::Format color_format) : context_(context)
{
    createPipelineLayout();
    createPipeline(color_format);
}


void GraphicsPipeline::createPipelineLayout()
{
    vk::PipelineLayoutCreateInfo pipeline_layout_create_info;
    layout_ = context_.getLogicalDevice().createPipelineLayout(pipeline_layout_create_info);
}


vk::raii::ShaderModule GraphicsPipeline::createShaderModule(const std::string& spirv_path) const
{
    std::vector<uint32_t> shader_code = readSpirv(spirv_path);
    vk::ShaderModuleCreateInfo shader_module_create_info = {
        .codeSize = shader_code.size() * sizeof(uint32_t),
        .pCode = shader_code.data()
    };

    return vk::raii::ShaderModule(context_.getLogicalDevice(), shader_module_create_info);
}

void GraphicsPipeline::createPipeline(vk::Format color_format)
{
    vk::raii::ShaderModule shader_module = createShaderModule("shaders/triangle.spv");
    
    vk::PipelineShaderStageCreateInfo vertex_shader_stage_info{
        .stage = vk::ShaderStageFlagBits::eVertex,
        .module = shader_module,
        .pName = "vertMain",
    };

    vk::PipelineShaderStageCreateInfo fragment_shader_stage_info{
        .stage = vk::ShaderStageFlagBits::eFragment,
        .module = shader_module,
        .pName = "fragMain",
    };

    vk::PipelineShaderStageCreateInfo shader_stages[] = {vertex_shader_stage_info, fragment_shader_stage_info};

    // Getting descriptions directly from Vertex class static functions.
    vk::VertexInputBindingDescription vertex_binding_description = Vertex::getBindingDescription();
    std::array<vk::VertexInputAttributeDescription, 2> vertex_attribute_descriptions =  Vertex::getAttributeDescriptions();
    
    vk::PipelineVertexInputStateCreateInfo vertex_input_info{
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &vertex_binding_description,
        .vertexAttributeDescriptionCount = static_cast<uint32_t>(vertex_attribute_descriptions.size()),
        .pVertexAttributeDescriptions = vertex_attribute_descriptions.data()
    };
    
    vk::PipelineInputAssemblyStateCreateInfo input_assembly{
        .topology = vk::PrimitiveTopology::eTriangleList
    };

    std::vector<vk::DynamicState> dynamic_states = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    vk::PipelineDynamicStateCreateInfo dynamic_state_create_info{
        .dynamicStateCount = static_cast<uint32_t>(dynamic_states.size()),
        .pDynamicStates = dynamic_states.data()
    };

    vk::PipelineViewportStateCreateInfo viewport_state{
        .viewportCount = 1,
        .scissorCount = 1,
    };

    vk::PipelineRasterizationStateCreateInfo rasterizer{
        .depthClampEnable = vk::False,
        .rasterizerDiscardEnable = vk::False,
        .polygonMode = vk::PolygonMode::eFill,
        .cullMode = vk::CullModeFlagBits::eBack,
        .frontFace = vk::FrontFace::eClockwise,
        .depthBiasEnable = vk::False,
        .lineWidth = 1.0f
    };

    vk::PipelineMultisampleStateCreateInfo multi_sampling_state_create_info{
        .rasterizationSamples = vk::SampleCountFlagBits::e1,
        .sampleShadingEnable = vk::False
    };

    vk::PipelineColorBlendAttachmentState color_blend_attachment_state{
        .blendEnable = vk::False,
        .colorWriteMask = vk::ColorComponentFlagBits::eR | 
                          vk::ColorComponentFlagBits::eG | 
                          vk::ColorComponentFlagBits::eB | 
                          vk::ColorComponentFlagBits::eA 
    };

    vk::PipelineColorBlendStateCreateInfo color_blending{
        .logicOpEnable = vk::False,
        .logicOp = vk::LogicOp::eCopy, 
        .attachmentCount = 1, 
        .pAttachments = &color_blend_attachment_state
    };

    vk::GraphicsPipelineCreateInfo graphics_pipeline_create_info{
        .stageCount = 2,
        .pStages = shader_stages,
        .pVertexInputState = &vertex_input_info,
        .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport_state,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multi_sampling_state_create_info,
        .pColorBlendState = &color_blending,
        .pDynamicState = &dynamic_state_create_info,
        .layout = *layout_,
        .renderPass = nullptr
    };

    vk::PipelineRenderingCreateInfo pipeline_rendering_create_info{
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &color_format,
    };

    vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> pipeline_create_info_chain = {
        graphics_pipeline_create_info, 
        pipeline_rendering_create_info
    };

    pipeline_ = vk::raii::Pipeline(context_.getLogicalDevice(), nullptr, pipeline_create_info_chain.get<vk::GraphicsPipelineCreateInfo>());

} 


const vk::raii::Pipeline& GraphicsPipeline::getPipeline() const
{
    return pipeline_;
}


void GraphicsPipeline::transitionImageLayout(
        vk::CommandBuffer command_buffer,
        vk::Image image,
        vk::ImageLayout old_layout,
        vk::ImageLayout new_layout,
        vk::PipelineStageFlags2 source_stage_mask,
        vk::AccessFlags2 source_access_mask,
        vk::PipelineStageFlags2 destination_stage_mask,
        vk::AccessFlags2 destination_access_mask
    )
{
    vk::ImageMemoryBarrier2 barrier = {
        .srcStageMask = source_stage_mask,
        .srcAccessMask = source_access_mask,
        .dstStageMask = destination_stage_mask,
        .dstAccessMask = destination_access_mask,
        .oldLayout = old_layout,
        .newLayout = new_layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = {
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }

    };

    vk::DependencyInfo dependency_info = {
        .dependencyFlags = {},  // using designated initializer.
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier
    };

    command_buffer.pipelineBarrier2(dependency_info);
}

void GraphicsPipeline::record(vk::CommandBuffer command_buffer, vk::Extent2D extent, vk::Image image,vk::ImageView image_view, const Mesh& mesh)
{
    vk::CommandBufferBeginInfo command_buffer_beging_info{};
    if (command_buffer.begin(&command_buffer_beging_info) != vk::Result::eSuccess)
    {
        throw std::runtime_error("unable to clean and start the command buffer begin");
    }

    transitionImageLayout(
        command_buffer,
        image,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        {},
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::AccessFlagBits2::eColorAttachmentWrite
    );
    
    vk::ClearValue clear_value{
        .color = { // vk::CleanColorValue struct. using the float initializer
            .float32 = std::array<float,4>{0.0f, 0.0f, 0.0f, 1.0f}
        }
    };
    
    vk::RenderingAttachmentInfo attachment_info{
        .imageView = image_view,
        .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .clearValue = clear_value
    };

    vk::RenderingInfo rendering_info{
        .renderArea = {
            .offset = {0, 0}, 
            .extent = extent
        },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &attachment_info
    };

    command_buffer.beginRendering(rendering_info);
    command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline_);
    command_buffer.setViewport(
        0, 
        vk::Viewport(0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height), 0.0f, 1.0f)
    );
    command_buffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), extent));
    mesh.bind(command_buffer);
    command_buffer.drawIndexed(mesh.getIndexCount(), 1, 0, 0, 0);
    command_buffer.endRendering();

    // transition the swap chain image to ePresenter source 
    transitionImageLayout(
        command_buffer,
        image,
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::ImageLayout::ePresentSrcKHR,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::AccessFlagBits2::eColorAttachmentWrite,
        vk::PipelineStageFlagBits2::eBottomOfPipe,
        {}
    );

    command_buffer.end();
}