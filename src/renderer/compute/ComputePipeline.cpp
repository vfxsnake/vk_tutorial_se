#include "ComputePipeline.h"

#include "core/VulkanContext.h"
#include "ParticleDescriptorLayout.h"
#include "utils/FileUtils.h"

ComputePipeline::ComputePipeline(
    const VulkanContext& context,
    const ParticleDescriptorLayout& particle_descriptor_layout
) : context_(context), 
    particleDescriptorLayout_(particle_descriptor_layout)
{
    createPipelineLayout();
    createPipeline();
}


void ComputePipeline::createPipelineLayout()
{
    vk::PipelineLayoutCreateInfo pipeline_layout_info{
        .setLayoutCount = 1,
        .pSetLayouts = &*(particleDescriptorLayout_.getLayout())
    };
    
    pipelineLayout_ = vk::raii::PipelineLayout(
        context_.getLogicalDevice(), 
        pipeline_layout_info
    );
}

void ComputePipeline::createPipeline()
{
    std::vector<uint32_t> shader_code = readSpirv("shaders/particles.spv");
    
    vk::ShaderModuleCreateInfo shader_module_create_info = {
        .codeSize = shader_code.size() * sizeof(uint32_t),
        .pCode = shader_code.data()
    };

    auto shader_module = vk::raii::ShaderModule(
        context_.getLogicalDevice(), 
        shader_module_create_info
    );

    vk::PipelineShaderStageCreateInfo compute_shader_stage_info{
        .stage = vk::ShaderStageFlagBits::eCompute, 
        .module = *shader_module, 
        .pName = "compMain"
    };

    vk::ComputePipelineCreateInfo pipeline_create_info{
        .stage = compute_shader_stage_info, 
        .layout = *pipelineLayout_
    };

    pipeline_ = vk::raii::Pipeline(context_.getLogicalDevice(), nullptr, pipeline_create_info);
}