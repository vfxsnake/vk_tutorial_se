#include "Renderer.h"
#include "core/VulkanContext.h"
#include "core/SwapChain.h"
#include "GraphicsPipeline.h"

Renderer::Renderer(VulkanContext& context) : context_(context)
{
    createCommandPool();
    initializeFrameData();
}

void Renderer::createCommandPool()
{
    vk::CommandPoolCreateInfo command_pool_create_info{
        .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,  // allows suing commandbuffer.reset() instead of reseting the entire pool.
        .queueFamilyIndex = context_.getQueueFamilyIndex()
    };

    commandPool_ = vk::raii::CommandPool(context_.getLogicalDevice(), command_pool_create_info);
}

void Renderer::initializeFrameData()  // initialize frame data (syncronization objects)
{
    // 
    for (RenderFrameSlot& render_frame_slot : renderFrameSlots_) // allready have the total slots but un initialized.
    {
        // refer to create command Buffer function from the Vulkan tutorial.
        vk::CommandBufferAllocateInfo command_buffer_allocate_info{
            .commandPool = commandPool_,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1
        };
        std::vector<vk::raii::CommandBuffer> buffers = context_.getLogicalDevice().allocateCommandBuffers(command_buffer_allocate_info);
        render_frame_slot.commandBuffer_ = std::move(buffers[0]); // pointing to the first commandBuffer from the command count.
        
        // refer to CreteSyncObjects() in the vulkan tutorial.
        render_frame_slot.imageAvailableSemaphore_ = vk::raii::Semaphore(context_.getLogicalDevice(), vk::SemaphoreCreateInfo());
        render_frame_slot.renderFinishedSemaphore_ = vk::raii::Semaphore(context_.getLogicalDevice(), vk::SemaphoreCreateInfo());
        render_frame_slot.inFlightFence_ = vk::raii::Fence(
            context_.getLogicalDevice(), {.flags = vk::FenceCreateFlagBits::eSignaled}
        );
    }
}


 bool Renderer::drawFrame(SwapChain& swap_chain, GraphicsPipeline& graphics_pipeline)
 {
    // CPU side fence: waiting for gpu task finishes
    vk::Result fence_result = context_.getLogicalDevice().waitForFences(
        *(renderFrameSlots_[currentFrame_].inFlightFence_), 
        vk::True, 
        UINT64_MAX
    );

    if(fence_result != vk::Result::eSuccess)
    {
        throw std::runtime_error("failed to wait for fence!");
    }

    // Acquiring Image
    // std::pair [vk::Result , uint32_t]
    auto [available_image_result, image_index] = swap_chain.get().acquireNextImage(
        UINT64_MAX,
        *(renderFrameSlots_[currentFrame_].imageAvailableSemaphore_),
        nullptr
    );

    if (available_image_result == vk::Result::eErrorOutOfDateKHR)
    {
        swap_chain.recreate();
        return false;
    }

    if (available_image_result != vk::Result::eSuccess && available_image_result != vk::Result::eSuboptimalKHR)
    {
        throw std::runtime_error("failed to acquire swap chain image!");
    }

    // reseting cpu fence to wait for the gpu
    context_.getLogicalDevice().resetFences(*(renderFrameSlots_[currentFrame_].inFlightFence_));
    
    // reseting the command buffer
    renderFrameSlots_[currentFrame_].commandBuffer_.reset();
    
    // recording the command buffer using the graphics_pipeline record command
    graphics_pipeline.record(
        renderFrameSlots_[currentFrame_].commandBuffer_,
        swap_chain.getExtent(),
        swap_chain.getImage(image_index),
        swap_chain.getImageView(image_index)
    );

    // submit to queue

    static constexpr vk::PipelineStageFlags wait_destination_stage_mask(vk::PipelineStageFlagBits::eColorAttachmentOutput);

    const vk::SubmitInfo submit_info{
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &*(renderFrameSlots_[currentFrame_].imageAvailableSemaphore_),
        .pWaitDstStageMask = &wait_destination_stage_mask,
        .commandBufferCount = 1,
        .pCommandBuffers = &*(renderFrameSlots_[currentFrame_].commandBuffer_),
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &*(renderFrameSlots_[currentFrame_].renderFinishedSemaphore_)
    };

    context_.getQueue().submit(submit_info, *(renderFrameSlots_[currentFrame_].inFlightFence_));

    // presenter
    const vk::PresentInfoKHR present_info{
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &*(renderFrameSlots_[currentFrame_].renderFinishedSemaphore_),
        .swapchainCount = 1,
        .pSwapchains = &*swap_chain.get(),
        .pImageIndices = &image_index
    };

    vk::Result presenter_result = context_.getQueue().presentKHR(present_info);
    
    if((presenter_result == vk::Result::eSuboptimalKHR) || (presenter_result == vk::Result::eErrorOutOfDateKHR ))
    {
        swap_chain.recreate();
        return false;
    }
    
    if (presenter_result != vk::Result::eSuccess)
    {
        throw std::runtime_error("failed to present swap chain image");
    }

    currentFrame_ = (currentFrame_ + 1) % MAX_FRAMES_IN_FLIGHT;
    return true;
 }