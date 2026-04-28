#include "Renderer.h"
#include "core/VulkanContext.h"
#include "core/SwapChain.h"
#include "GraphicsPipeline.h"

#include <stdexcept>

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


void Renderer::createFinishedSemaphores(uint32_t image_count)
{
    renderFinishedSemaphores_.clear();
    renderFinishedSemaphores_.reserve(image_count);
    for (uint32_t i = 0; i < image_count; i++)
    {
        // emplace_back will call vk::raii::Semaphore's constructor with the parameters (logicalDevice, SemaphoreCreateInfo).           
        renderFinishedSemaphores_.emplace_back(context_.getLogicalDevice(), vk::SemaphoreCreateInfo()); 
    }
}


void Renderer::initializePerImageResources(uint32_t image_count)
{
    createFinishedSemaphores(image_count);
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
        render_frame_slot.inFlightFence_ = vk::raii::Fence(
            context_.getLogicalDevice(), {.flags = vk::FenceCreateFlagBits::eSignaled}
        );
    }
}


 bool Renderer::drawFrame(SwapChain& swap_chain, GraphicsPipeline& graphics_pipeline, const Mesh& mesh)
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
        swap_chain.getImageView(image_index),
        mesh
    );

    static constexpr vk::PipelineStageFlags wait_destination_stage_mask(vk::PipelineStageFlagBits::eColorAttachmentOutput);

    const vk::SubmitInfo submit_info{
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &*(renderFrameSlots_[currentFrame_].imageAvailableSemaphore_),
        .pWaitDstStageMask = &wait_destination_stage_mask,
        .commandBufferCount = 1,
        .pCommandBuffers = &*(renderFrameSlots_[currentFrame_].commandBuffer_),
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &*(renderFinishedSemaphores_[image_index])
    };

    context_.getQueue().submit(submit_info, *(renderFrameSlots_[currentFrame_].inFlightFence_));

    // presenter
    const vk::PresentInfoKHR present_info{
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &*(renderFinishedSemaphores_[image_index]),
        .swapchainCount = 1,
        .pSwapchains = &*swap_chain.get(),
        .pImageIndices = &image_index
    };

    vk::Result presenter_result;
    try
    {
        presenter_result = context_.getQueue().presentKHR(present_info);
    }
    catch (const vk::OutOfDateKHRError&)
    {                                                                                                                                 
        return false;
    }
    
    if(presenter_result == vk::Result::eSuboptimalKHR)
    {
        return false;
    }
    
    if (presenter_result != vk::Result::eSuccess)
    {
        throw std::runtime_error("failed to present swap chain image");
    }

    currentFrame_ = (currentFrame_ + 1) % MAX_FRAMES_IN_FLIGHT;
    return true;
 }


 uint32_t Renderer::findMemoryType(uint32_t type_filter, vk::MemoryPropertyFlags properties) const
 {
    vk::PhysicalDeviceMemoryProperties memory_properties = context_.getPhysicalDevice().getMemoryProperties();
    for (uint32_t i = 0; i < memory_properties.memoryTypeCount; i++)
    {
        if (
            (type_filter & (1 << i)) && 
            (memory_properties.memoryTypes[i].propertyFlags & properties) == properties
        )
        {
            return i;
        }
    }

    throw std::runtime_error("failed to find suitable memory type!");
 }


 std::pair<vk::raii::Buffer, vk::raii::DeviceMemory> Renderer::createBuffer(
        vk::DeviceSize size, 
        vk::BufferUsageFlags usage, 
        vk::MemoryPropertyFlags memory_properties
    )
{
    vk::BufferCreateInfo create_buffer_info{
        .size = size,
        .usage = usage,
        .sharingMode = vk::SharingMode::eExclusive
    };

    vk::raii::Buffer buffer = vk::raii::Buffer(context_.getLogicalDevice(), create_buffer_info);
    vk::MemoryRequirements memory_requirements = buffer.getMemoryRequirements();
    vk::MemoryAllocateInfo memory_allocate_info{
        .allocationSize = memory_requirements.size,
        .memoryTypeIndex = findMemoryType(memory_requirements.memoryTypeBits, memory_properties)
    };

    vk::raii::DeviceMemory buffer_memory = vk::raii::DeviceMemory(context_.getLogicalDevice(), memory_allocate_info);
    buffer.bindMemory(*buffer_memory, 0);
    return {std::move(buffer), std::move(buffer_memory)};
}


void Renderer::copyBuffer(
        vk::raii::Buffer &source_buffer, 
        vk::raii::Buffer &destination_buffer, 
        vk::DeviceSize size
    )
{
    vk::CommandBufferAllocateInfo buffer_allocate_info{
        .commandPool = *commandPool_,
        .level = vk::CommandBufferLevel::ePrimary, 
        .commandBufferCount = 1
    };

    vk::raii::CommandBuffer command_copy_buffer = std::move(context_.getLogicalDevice().allocateCommandBuffers(buffer_allocate_info).front());
    vk::CommandBufferBeginInfo command_buffer_begin_info{.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit};
    command_copy_buffer.begin(command_buffer_begin_info);
    vk::BufferCopy buffer_copy{
        .srcOffset = 0, 
        .dstOffset = 0,
        .size = size
    };
    command_copy_buffer.copyBuffer(*source_buffer, *destination_buffer, buffer_copy);
    command_copy_buffer.end();
    vk::SubmitInfo submit_info{
        .commandBufferCount = 1,
        .pCommandBuffers = &*command_copy_buffer
    };
    context_.getQueue().submit(submit_info, nullptr);
    context_.getQueue().waitIdle();
}


std::pair<vk::raii::Buffer, vk::raii::DeviceMemory> Renderer::uploadBufferToDevice(
        const void* data,
        vk::DeviceSize buffer_size, 
        vk::BufferUsageFlags usage
    )
{
    // pulling the vk::raii::buffer and vk::raii::DeviceMemory
    auto [staging_buffer, staging_buffer_memory] = createBuffer(
        buffer_size,
        vk::BufferUsageFlagBits::eTransferSrc, 
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
    );

    void *data_staging = staging_buffer_memory.mapMemory(0, buffer_size);
    memcpy(data_staging, data, buffer_size);
    staging_buffer_memory.unmapMemory();

    auto [buffer, buffer_memory] = createBuffer(
        buffer_size, 
        usage | vk::BufferUsageFlagBits::eTransferDst,
        vk::MemoryPropertyFlagBits::eDeviceLocal
    );

    copyBuffer(staging_buffer, buffer, buffer_size);
    return {std::move(buffer), std::move(buffer_memory)};
}


Mesh Renderer::createMesh(const std::vector<Vertex>& vertices, const std::vector<uint16_t>& indices)
{
    // Vertex buffer handling
    vk::DeviceSize vertex_buffer_size = sizeof(Vertex) * vertices.size();  // on the tutorial sizeof(vertices[0]) works the same as it pulls the type from the array address
    auto [vertex_buffer, vertex_buffer_memory] = uploadBufferToDevice(
        vertices.data(), 
        vertex_buffer_size, 
        vk::BufferUsageFlagBits::eVertexBuffer
    );

    // Index buffer handling
    vk::DeviceSize index_buffer_size = sizeof(uint16_t) * indices.size();
    auto [index_buffer, index_buffer_memory] = uploadBufferToDevice(
        indices.data(), 
        index_buffer_size, 
        vk::BufferUsageFlagBits::eIndexBuffer
    );

    return Mesh(
        std::move(vertex_buffer), 
        std::move(vertex_buffer_memory), 
        static_cast<uint32_t>(vertices.size()),
        std::move(index_buffer),
        std::move(index_buffer_memory),
        static_cast<uint32_t>(indices.size()),
        vk::IndexType::eUint16
    );
}