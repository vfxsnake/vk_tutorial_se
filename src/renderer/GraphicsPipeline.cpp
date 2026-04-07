#include "GraphicsPipeline.h"
#include "core/VulkanContext.h"
#include "utils/FileUtils.h"

#include <stdexcept>


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