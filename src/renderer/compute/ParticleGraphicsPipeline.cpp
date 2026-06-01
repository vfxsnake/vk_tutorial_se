#include "ParticleGraphicsPipeline.h"

#include "core/VulkanContext.h"
#include "ParticleDescriptorLayout.h"
#include "utils/FileUtils.h"
#include "renderer/buffers/Particle.h"

#include <array>

ParticleGraphicsPipeline::ParticleGraphicsPipeline(
    const VulkanContext& context, 
    const ParticleDescriptorLayout& particle_descriptor_layout,
    vk::Format color_format,
    vk::Format depth_format,
    vk::SampleCountFlagBits samples
) :
    context_(context),
    particleDescriptorLayout_(particle_descriptor_layout),
    depthFormat_(depth_format),
    msaaSamples_(samples)
{
    createPipelineLayout();
    createPipeline(color_format);
}

void ParticleGraphicsPipeline::createPipelineLayout()
{
    vk::DescriptorSetLayout descriptor_layout = *particleDescriptorLayout_.getLayout();
    vk::PipelineLayoutCreateInfo pipeline_layout_create_info{
        .setLayoutCount = 1,
        .pSetLayouts = &descriptor_layout
    };
    layout_ = context_.getLogicalDevice().createPipelineLayout(pipeline_layout_create_info);
}

void ParticleGraphicsPipeline::createPipeline(vk::Format color_format)
{
    vk::raii::ShaderModule shader_module = createShaderModule("shaders/particles.spv");
    
    vk::PipelineShaderStageCreateInfo vertex_shader_stage_info{
        .stage = vk::ShaderStageFlagBits::eVertex,
        .module = *shader_module,
        .pName = "vertMain",
    };

    vk::PipelineShaderStageCreateInfo fragment_shader_stage_info{
        .stage = vk::ShaderStageFlagBits::eFragment,
        .module = *shader_module,
        .pName = "fragMain",
    };

    vk::PipelineShaderStageCreateInfo shader_stages[] = {vertex_shader_stage_info, fragment_shader_stage_info};

    // Getting descriptions directly from Vertex class static functions.
    vk::VertexInputBindingDescription particle_binding_description = Particle::getBindingDescription();
    auto vertex_attribute_descriptions =  Particle::getAttributeDescriptions(); // std::array<vk::VertexInputAttributeDescription, n> n: number of attributes 
    
    vk::PipelineVertexInputStateCreateInfo particle_input_info{
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &particle_binding_description,
        .vertexAttributeDescriptionCount = static_cast<uint32_t>(vertex_attribute_descriptions.size()),
        .pVertexAttributeDescriptions = vertex_attribute_descriptions.data()
    };
    
    vk::PipelineInputAssemblyStateCreateInfo input_assembly{
        .topology = vk::PrimitiveTopology::ePointList,
        .primitiveRestartEnable = false
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
        .cullMode = vk::CullModeFlagBits::eNone,
        .depthBiasEnable = vk::False,
        .lineWidth = 1.0f
    };

    vk::PipelineMultisampleStateCreateInfo multi_sampling_state_create_info{
        .rasterizationSamples = msaaSamples_,
        .sampleShadingEnable = vk::False
    };

    vk::PipelineColorBlendAttachmentState color_blend_attachment_state{
        .blendEnable = vk::True,
        .srcColorBlendFactor = vk::BlendFactor::eSrcAlpha,
        .dstColorBlendFactor = vk::BlendFactor::eOne,
        .colorBlendOp       = vk::BlendOp::eAdd,
        .srcAlphaBlendFactor = vk::BlendFactor::eOne,
        .dstAlphaBlendFactor = vk::BlendFactor::eZero,
        .alphaBlendOp       = vk::BlendOp::eAdd,
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

    vk::PipelineDepthStencilStateCreateInfo pipeline_depth_stencil_state_create_info{
        .depthTestEnable = vk::True,
        .depthWriteEnable = vk::False,
        .depthCompareOp = vk::CompareOp::eLess,
        .depthBoundsTestEnable = vk::False,
        .stencilTestEnable = vk::False
    };

    vk::GraphicsPipelineCreateInfo graphics_pipeline_create_info{
        .stageCount = 2,
        .pStages = shader_stages,
        .pVertexInputState = &particle_input_info,
        .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport_state,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multi_sampling_state_create_info,
        .pDepthStencilState = &pipeline_depth_stencil_state_create_info,
        .pColorBlendState = &color_blending,
        .pDynamicState = &dynamic_state_create_info,
        .layout = *layout_,
        .renderPass = nullptr
    };

    vk::PipelineRenderingCreateInfo pipeline_rendering_create_info{
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &color_format,
        .depthAttachmentFormat = depthFormat_
    };

    vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> pipeline_create_info_chain = {
        graphics_pipeline_create_info, 
        pipeline_rendering_create_info
    };

    pipeline_ = vk::raii::Pipeline(context_.getLogicalDevice(), nullptr, pipeline_create_info_chain.get<vk::GraphicsPipelineCreateInfo>());

}

vk::raii::ShaderModule ParticleGraphicsPipeline::createShaderModule(const std::string& spirv_path) const
{
    std::vector<uint32_t> shader_code = readSpirv(spirv_path);
    vk::ShaderModuleCreateInfo shader_module_create_info = {
        .codeSize = shader_code.size() * sizeof(uint32_t),
        .pCode = shader_code.data()
    };

    return vk::raii::ShaderModule(context_.getLogicalDevice(), shader_module_create_info);
}

void ParticleGraphicsPipeline::transitionImageLayout(
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


void ParticleGraphicsPipeline::record(
    vk::CommandBuffer command_buffer,
    vk::Extent2D extent,
    vk::Image image,
    vk::ImageView color_view,
    vk::ImageView depth_view,
    vk::ImageView msaa_view,
    const vk::raii::DescriptorSet& compute_descriptor_set,
    vk::Buffer particle_buffer,
    uint32_t particle_count
)
{
        
    vk::RenderingAttachmentInfo attachment_info{
        .imageView = msaa_view,
        .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .resolveMode = vk::ResolveModeFlagBits::eAverage,
        .resolveImageView = color_view,
        .resolveImageLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .loadOp = vk::AttachmentLoadOp::eLoad,
        .storeOp = vk::AttachmentStoreOp::eDontCare
    };

    vk::RenderingAttachmentInfo depth_attachment_info{
        .imageView = depth_view,
        .imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
        .loadOp = vk::AttachmentLoadOp::eLoad,
        .storeOp = vk::AttachmentStoreOp::eDontCare
    };

    vk::RenderingInfo rendering_info{
        .renderArea = {
            .offset = {0, 0}, 
            .extent = extent
        },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &attachment_info,
        .pDepthAttachment = &depth_attachment_info
    };

    command_buffer.beginRendering(rendering_info);
    command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline_);
    
    command_buffer.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics,
        *layout_,
        0, 
        {*compute_descriptor_set},
        {}
    );

    command_buffer.setViewport(
        0, 
        vk::Viewport(
            0.0f, // x 
            0.0f, // y
            static_cast<float>(extent.width), // width
            static_cast<float>(extent.height), // height
            0.0f, // min depth
            1.0f // max depth
        )
    );
    command_buffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), extent));
    command_buffer.bindVertexBuffers(0, {particle_buffer}, {0});
    command_buffer.draw(particle_count, 1, 0, 0);
    
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
  
}