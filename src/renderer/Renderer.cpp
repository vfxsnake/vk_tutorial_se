#include "Renderer.h"
#include "core/VulkanContext.h"
#include "core/SwapChain.h"
#include "GraphicsPipeline.h"
#include "descriptors/FrameDescriptorLayout.h"
#include "descriptors/TextureDescriptorLayout.h"
#include "compute/ComputePipeline.h"
#include "compute/ComputeUniformBufferObject.h"
#include "compute/ParticleDescriptorLayout.h"
#include "compute/ParticleGraphicsPipeline.h"

#include <stdexcept>
#include <cmath>
#include <algorithm>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

Renderer::Renderer(
    VulkanContext& context, 
    const TextureDescriptorLayout& texture_descriptor_layout,
    const FrameDescriptorLayout& frame_descriptor_layout,
    const ComputePipeline& compute_pipeline,
    const ParticleGraphicsPipeline& particle_graphics_pipeline,
    const ParticleDescriptorLayout& particle_descriptor_layout
) : context_(context), 
    frameDescriptorLayout_(frame_descriptor_layout),
    textureDescriptorLayout_(texture_descriptor_layout),
    computePipeline_(compute_pipeline),
    particleGraphicsPipeline_(particle_graphics_pipeline),
    particleDescriptorLayout_(particle_descriptor_layout)
{
    createCommandPool();
    createDescriptorPool();
    createTextureDescriptorPool();
    initializeFrameData();
    createComputeDescriptorPool();
}

void Renderer::createCommandPool()
{
    vk::CommandPoolCreateInfo command_pool_create_info{
        .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,  // allows the use commandbuffer.reset() instead of reseting the entire pool.
        .queueFamilyIndex = context_.getQueueFamilyIndex()
    };

    commandPool_ = vk::raii::CommandPool(context_.getLogicalDevice(), command_pool_create_info);
}


void Renderer::createDescriptorPool()
{
    vk::DescriptorPoolSize descriptor_pool_size{
        .type = vk::DescriptorType::eUniformBuffer,
        .descriptorCount = MAX_NUMBER_OF_OBJECTS * MAX_FRAMES_IN_FLIGHT
    };

    vk::DescriptorPoolCreateInfo descriptor_pool_create_info{
        .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, 
        .maxSets = MAX_NUMBER_OF_OBJECTS * MAX_FRAMES_IN_FLIGHT,
        .poolSizeCount = 1,
        .pPoolSizes = &descriptor_pool_size  
    };

    descriptorPool_ = vk::raii::DescriptorPool(context_.getLogicalDevice(), descriptor_pool_create_info);
}

void Renderer::createComputeDescriptorPool()
{
    vk::DescriptorPoolSize descriptor_pool_size_1{
        .type = vk::DescriptorType::eUniformBuffer,
        .descriptorCount = MAX_FRAMES_IN_FLIGHT
    };

    vk::DescriptorPoolSize descriptor_pool_size_2{
        .type = vk::DescriptorType::eStorageBuffer,
        .descriptorCount = MAX_FRAMES_IN_FLIGHT * 2
    };

    std::array<vk::DescriptorPoolSize, 2> descriptor_pool_sizes{descriptor_pool_size_1, descriptor_pool_size_2};
    vk::DescriptorPoolCreateInfo descriptor_pool_create_info{
        .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        .maxSets = MAX_FRAMES_IN_FLIGHT,
        .poolSizeCount = static_cast<uint32_t>(descriptor_pool_sizes.size()),
        .pPoolSizes = descriptor_pool_sizes.data()
    };
    computeDescriptorPool_ = vk::raii::DescriptorPool(context_.getLogicalDevice(), descriptor_pool_create_info);
}


void Renderer::createTextureDescriptorPool()
{
    vk::DescriptorPoolSize texture_descriptor_pool_size{
        .type = vk::DescriptorType::eCombinedImageSampler,
        .descriptorCount = 1
    };

    vk::DescriptorPoolCreateInfo texture_descriptor_pool_create_info{
        .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        .maxSets = 1,
        .poolSizeCount = 1,
        .pPoolSizes = &texture_descriptor_pool_size
    };

    textureDescriptorPool_ = vk::raii::DescriptorPool(context_.getLogicalDevice(), texture_descriptor_pool_create_info);
    
    vk::DescriptorSetAllocateInfo descriptor_set_allocate_info{
        .descriptorPool = *textureDescriptorPool_,
        .descriptorSetCount = 1,
        .pSetLayouts = &*(textureDescriptorLayout_.getLayout()),
    };

    std::vector<vk::raii::DescriptorSet> descriptor_sets = context_.getLogicalDevice().allocateDescriptorSets(descriptor_set_allocate_info);
    textureDescriptorSet_ = std::move(descriptor_sets.front());
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
    for (RenderFrameSlot& render_frame_slot : renderFrameSlots_) // allready have the total slots but un initialized.
    {
        vk::CommandBufferAllocateInfo command_buffer_allocate_info{
            .commandPool = *commandPool_,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1
        };
        
        std::vector<vk::raii::CommandBuffer> buffers = context_.getLogicalDevice().allocateCommandBuffers(command_buffer_allocate_info);
        render_frame_slot.commandBuffer_ = std::move(buffers[0]); 
        
        render_frame_slot.imageAvailableSemaphore_ = vk::raii::Semaphore(context_.getLogicalDevice(), vk::SemaphoreCreateInfo());
        
        render_frame_slot.inFlightFence_ = vk::raii::Fence(
            context_.getLogicalDevice(), {.flags = vk::FenceCreateFlagBits::eSignaled}
        );
    }
}


bool Renderer::drawFrame(
    SwapChain& swap_chain, 
    GraphicsPipeline& graphics_pipeline, 
    std::span<const UniformBufferObject> uniform_buffer_objects,
    vk::ImageView depth_image_view,
    vk::ImageView msaa_color_image_view,
    float delta_time
)
{   
    // Compute Block
    // compute wait of fence
    vk::Result compute_fence_result = context_.getLogicalDevice().waitForFences(
        *(computeFrameSlots_[currentFrame_].computeInflightFence_), 
        vk::True, 
        UINT64_MAX
    );

    if(compute_fence_result != vk::Result::eSuccess)
    {
        throw std::runtime_error("failed to wait for compute fence!");
    }

    computeFrameSlots_[currentFrame_].updateComputeUniformBuffer(
        {delta_time} // right hand side initialized ComputeUniformBufferObject
    );

    context_.getLogicalDevice().resetFences(*(computeFrameSlots_[currentFrame_].computeInflightFence_));
    computeFrameSlots_[currentFrame_].computeCommandBuffer_.reset();
    computePipeline_.record(
        *(computeFrameSlots_[currentFrame_].computeCommandBuffer_),
        computeFrameSlots_[currentFrame_].descriptorSet_,
        particleCount_
    );

    const vk::SubmitInfo submit_compute_info{
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = nullptr,
        .pWaitDstStageMask = nullptr,
        .commandBufferCount = 1,
        .pCommandBuffers = &*(computeFrameSlots_[currentFrame_].computeCommandBuffer_),
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &*(computeFrameSlots_[currentFrame_].computeFinishedSemaphore_)
    };

    context_.getQueue().submit(submit_compute_info, *(computeFrameSlots_[currentFrame_].computeInflightFence_));

    // Graphics block 
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

    // updating uniform buffers
    for (uint32_t i=0; i < objectRenderDataEntries_.size(); i++)
    {
        objectRenderDataEntries_[i].updateUniformBuffer(currentFrame_, uniform_buffer_objects[i]);
    }
    
    // reseting the command buffer
    renderFrameSlots_[currentFrame_].commandBuffer_.reset();
    
    // starting the buffer .begin
    vk::CommandBufferBeginInfo command_buffer_begin_info{};
    renderFrameSlots_[currentFrame_].commandBuffer_.begin(command_buffer_begin_info);
    
    std::vector<vk::DescriptorSet> descriptor_sets;
    std::vector<const Mesh*> mesh_pointers;

    for (uint32_t index = 0; index < objectRenderDataEntries_.size(); index ++)
    {
        descriptor_sets.push_back(*(objectRenderDataEntries_[index].descriptorSets_[currentFrame_]));
        mesh_pointers.push_back(&meshes_[objectMeshIndices_[index]]); 
    }
    
    // recording the command buffer using the graphics_pipeline record command
    graphics_pipeline.record(
        renderFrameSlots_[currentFrame_].commandBuffer_,
        descriptor_sets,
        textureDescriptorSet_,
        swap_chain.getExtent(),
        swap_chain.getImage(image_index),
        swap_chain.getImageView(image_index),
        msaa_color_image_view,
        mesh_pointers,
        depth_image_view
    );

    particleGraphicsPipeline_.record(
        renderFrameSlots_[currentFrame_].commandBuffer_,
        swap_chain.getExtent(),
        swap_chain.getImage(image_index),
        swap_chain.getImageView(image_index),
        depth_image_view,
        msaa_color_image_view,
        computeFrameSlots_[currentFrame_].descriptorSet_,
        *computeFrameSlots_[currentFrame_].particleBuffer_,
        particleCount_
    );

    renderFrameSlots_[currentFrame_].commandBuffer_.end();

    std::array<vk::PipelineStageFlags, 2> wait_destination_stage_masks = {
        vk::PipelineStageFlagBits::eColorAttachmentOutput,
        vk::PipelineStageFlagBits::eVertexInput
    };

    std::array<vk::Semaphore, 2> wait_semaphores = {
        *(renderFrameSlots_[currentFrame_].imageAvailableSemaphore_),
        *(computeFrameSlots_[currentFrame_].computeFinishedSemaphore_)
    };

    const vk::SubmitInfo submit_info{
        .waitSemaphoreCount = 2,
        .pWaitSemaphores = wait_semaphores.data(),
        .pWaitDstStageMask = wait_destination_stage_masks.data(),
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


Mesh Renderer::createMesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
{
    // Vertex buffer handling
    vk::DeviceSize vertex_buffer_size = sizeof(Vertex) * vertices.size();  // on the tutorial sizeof(vertices[0]) works the same as it pulls the type from the array address
    auto [vertex_buffer, vertex_buffer_memory] = uploadBufferToDevice(
        vertices.data(), 
        vertex_buffer_size, 
        vk::BufferUsageFlagBits::eVertexBuffer
    );

    // Index buffer handling
    vk::DeviceSize index_buffer_size = sizeof(uint32_t) * indices.size();
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
        vk::IndexType::eUint32
    );
}


uint32_t Renderer::addMesh(Mesh&& mesh)
{
    meshes_.push_back(std::move(mesh));
    return static_cast<uint32_t>(meshes_.size() -1);
}


void Renderer::createObjectRenderData(uint32_t mesh_index)
{
    ObjectRenderData render_data;

    for (uint32_t i=0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        auto [uniform_buffer, uniform_buffer_memory] = createBuffer(
            sizeof(UniformBufferObject), 
            vk::BufferUsageFlagBits::eUniformBuffer,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
        );
        void* mapped = uniform_buffer_memory.mapMemory(0, sizeof(UniformBufferObject));

        // allocating descriptor set
        vk::DescriptorSetAllocateInfo descriptor_set_allocate_info{
            .descriptorPool = descriptorPool_,
            .descriptorSetCount = 1,
            .pSetLayouts = &*(frameDescriptorLayout_.getLayout()),
        };
        std::vector<vk::raii::DescriptorSet> descriptor_sets = context_.getLogicalDevice().allocateDescriptorSets(descriptor_set_allocate_info);
    
        render_data.uniformBuffers_.push_back(std::move(uniform_buffer));
        render_data.uniformBufferMemories_.push_back(std::move(uniform_buffer_memory));
        render_data.uniformBufferMemoriesMapped_.push_back(mapped);
        render_data.descriptorSets_.push_back(std::move(descriptor_sets.front()));

        vk::DescriptorBufferInfo descriptor_buffer_info{
            .buffer = *render_data.uniformBuffers_.back(), // back() uses the lates pushed element in the vector
            .offset = 0,
            .range = sizeof(UniformBufferObject)
        };

        vk::WriteDescriptorSet write_descriptor_set{
            .dstSet = *render_data.descriptorSets_.back(),
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eUniformBuffer,
            .pBufferInfo = &descriptor_buffer_info
        };

        context_.getLogicalDevice().updateDescriptorSets({write_descriptor_set}, {});
    }
    objectRenderDataEntries_.push_back(std::move(render_data));
    objectMeshIndices_.push_back(mesh_index);
}


vk::raii::CommandBuffer Renderer::beginSingleTimeCommands()
{
    vk::CommandBufferAllocateInfo command_buffer_allocate_info{
        .commandPool = commandPool_,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = 1
    };

    vk::raii::CommandBuffer command_buffer = std::move(
                vk::raii::CommandBuffers(context_.getLogicalDevice(), 
                command_buffer_allocate_info).front()
            );
    
    vk::CommandBufferBeginInfo command_buffer_begin_info{
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
    };

    command_buffer.begin(command_buffer_begin_info);

    return std::move(command_buffer);
}


void Renderer::endSingleTimeCommands(vk::raii::CommandBuffer command_buffer)
{
    command_buffer.end();
    vk::SubmitInfo submit_info{
        .commandBufferCount = 1,
        .pCommandBuffers = &*command_buffer
    };

    context_.getQueue().submit(submit_info, nullptr);
    context_.getQueue().waitIdle();
}


std::pair<vk::raii::Image, vk::raii::DeviceMemory> Renderer::createImage(
    uint32_t width, 
    uint32_t height,
    uint32_t mip_levels,
    vk::SampleCountFlagBits num_samples,
    vk::Format format, 
    vk::ImageTiling tiling,
    vk::ImageUsageFlags usage_flags,
    vk::MemoryPropertyFlags memory_property_flags
)
{
    vk::ImageCreateInfo image_create_info{
        .imageType = vk::ImageType::e2D,
        .format = format,
        .extent = {.width = width, .height = height, .depth = 1},
        .mipLevels = mip_levels,
        .arrayLayers = 1,
        .samples = num_samples,
        .tiling = tiling,
        .usage = usage_flags,
        .sharingMode = vk::SharingMode::eExclusive
    };

    vk::raii::Image image = vk::raii::Image(context_.getLogicalDevice(), image_create_info);
    vk::MemoryRequirements memory_requirements = image.getMemoryRequirements();
    
    vk::MemoryAllocateInfo memory_allocate_info{
        .allocationSize = memory_requirements.size,
        .memoryTypeIndex = findMemoryType(memory_requirements.memoryTypeBits, memory_property_flags)
    };

    vk::raii::DeviceMemory image_memory = vk::raii::DeviceMemory(context_.getLogicalDevice(), memory_allocate_info);
    image.bindMemory(*image_memory, 0);

    return {std::move(image), std::move(image_memory)};
}


vk::raii::ImageView Renderer::createImageView(
    vk::Image image,
    vk::Format format,
    vk::ImageAspectFlags aspect_flags,
    uint32_t level_count
)
{
    vk::ImageViewCreateInfo image_view_create_info{
        .image = image,
        .viewType = vk::ImageViewType::e2D,
        .format = format,
        .subresourceRange = {
            .aspectMask = aspect_flags,
            .baseMipLevel = 0,
            .levelCount = level_count,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    return vk::raii::ImageView(context_.getLogicalDevice(), image_view_create_info);
} 


void Renderer::transitionImageLayout(
        vk::Image image,
        vk::ImageLayout old_layout,
        vk::ImageLayout new_layout,
        vk::ImageAspectFlags aspect_flags,
        uint32_t level_count
    )
{
    vk::ImageMemoryBarrier image_memory_barrier{
        .oldLayout = old_layout,
        .newLayout = new_layout,
        .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
        .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
        .image = image,
        .subresourceRange = {
            .aspectMask = aspect_flags,
            .baseMipLevel = 0,
            .levelCount = level_count,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    vk::PipelineStageFlags source_stage;
    vk::PipelineStageFlags destination_stage;

    if (old_layout == vk::ImageLayout::eUndefined && new_layout == vk::ImageLayout::eTransferDstOptimal)
    {
        image_memory_barrier.srcAccessMask = {};
        image_memory_barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

        source_stage = vk::PipelineStageFlagBits::eTopOfPipe;
        destination_stage = vk::PipelineStageFlagBits::eTransfer;
    }
    
    else if (old_layout == vk::ImageLayout::eTransferDstOptimal && new_layout == vk::ImageLayout::eShaderReadOnlyOptimal)
    {
        image_memory_barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        image_memory_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        source_stage = vk::PipelineStageFlagBits::eTransfer;
        destination_stage = vk::PipelineStageFlagBits::eFragmentShader;
    }
    
    else if (old_layout == vk::ImageLayout::eUndefined && new_layout == vk::ImageLayout::eDepthAttachmentOptimal)
    {
        image_memory_barrier.srcAccessMask = {};
        image_memory_barrier.dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite;
        source_stage = vk::PipelineStageFlagBits::eTopOfPipe;
        destination_stage = vk::PipelineStageFlagBits::eEarlyFragmentTests;
    }

    else
    {
        throw std::invalid_argument("unsupported layout transition!");
    }

    vk::raii::CommandBuffer command_buffer = beginSingleTimeCommands();
    command_buffer.pipelineBarrier(source_stage, destination_stage, {}, {}, {}, image_memory_barrier);
    endSingleTimeCommands(std::move(command_buffer));
}


void Renderer::copyBufferToImage(vk::Buffer buffer, vk::Image image, uint32_t width, uint32_t height)
{
    vk::BufferImageCopy buffer_image_copy{
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = {
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .mipLevel = 0,
            .baseArrayLayer = 0, 
            .layerCount = 1
        },
        .imageOffset = {0, 0, 0},
        .imageExtent = {width, height,1}
    };

    vk::raii::CommandBuffer command_buffer = beginSingleTimeCommands();
    command_buffer.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, buffer_image_copy);
    endSingleTimeCommands(std::move(command_buffer));
}


vk::raii::Sampler Renderer::createSampler()
{
    vk::PhysicalDeviceProperties physical_device_properties = context_.getPhysicalDevice().getProperties();
    vk::SamplerCreateInfo sampler_create_info{
        .magFilter = vk::Filter::eLinear,
        .minFilter = vk::Filter::eLinear,
        .mipmapMode = vk::SamplerMipmapMode::eLinear,
        .addressModeU = vk::SamplerAddressMode::eRepeat,
        .addressModeV = vk::SamplerAddressMode::eRepeat,
        .addressModeW = vk::SamplerAddressMode::eRepeat,
        .mipLodBias = 0.0f,
        .anisotropyEnable = vk::True,
        .maxAnisotropy = physical_device_properties.limits.maxSamplerAnisotropy,
        .compareEnable = vk::False,
        .compareOp = vk::CompareOp::eAlways,
        .minLod = 0.0f,
        .maxLod = vk::LodClampNone
    };

    return vk::raii::Sampler(context_.getLogicalDevice(), sampler_create_info);
}


Texture Renderer::createTexture(const std::string& path)
{
    int texture_width;
    int texture_height;
    [[maybe_unused]] int texture_channels;

    stbi_uc *pixel_data = stbi_load(path.c_str(), &texture_width, &texture_height, &texture_channels, STBI_rgb_alpha);
    if (!pixel_data)
    {
        throw std::runtime_error("failed to load texutre image: " + path);
    }
    
    uint32_t mip_levels = static_cast<uint32_t>(std::floor(std::log2(std::max(texture_width, texture_height)))) + 1;

    vk::DeviceSize image_size = texture_width * texture_height * 4;

    auto [staging_buffer, staging_buffer_memory] = createBuffer(
        image_size, 
        vk::BufferUsageFlagBits::eTransferSrc ,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
    );

    void *data = staging_buffer_memory.mapMemory(0, image_size);
    memcpy(data, pixel_data, image_size);
    staging_buffer_memory.unmapMemory();

    stbi_image_free(pixel_data);

    auto [texture_image, texture_image_memory] = createImage(
        static_cast<uint32_t>(texture_width),
        static_cast<uint32_t>(texture_height),
        mip_levels,
        vk::SampleCountFlagBits::e1,
        vk::Format::eR8G8B8A8Srgb,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
        vk::MemoryPropertyFlagBits::eDeviceLocal
    );

    transitionImageLayout(
        *texture_image, 
        vk::ImageLayout::eUndefined, 
        vk::ImageLayout::eTransferDstOptimal,
        vk::ImageAspectFlagBits::eColor,
        mip_levels
    );
    
    copyBufferToImage(
        *staging_buffer, 
        *texture_image, 
        static_cast<uint32_t>(texture_width), static_cast<uint32_t>(texture_height)
    );
    
    generateMipmaps(
        *texture_image,
        vk::Format::eR8G8B8A8Srgb,
        static_cast<uint32_t>(texture_width),
        static_cast<uint32_t> (texture_height),
        mip_levels
    );
    
    vk::raii::ImageView image_view = createImageView(
        *texture_image, 
        vk::Format::eR8G8B8A8Srgb, 
        vk::ImageAspectFlagBits::eColor, 
        mip_levels
    );
    
    vk::raii::Sampler sampler = createSampler(); 
    
    return Texture(
        std::move(texture_image),
        std::move(texture_image_memory),
        std::move(image_view),
        std::move(sampler),
        mip_levels
    );    
}


void Renderer::bindTextureToDescriptor(const Texture& texture)
{
    vk::DescriptorImageInfo descriptor_image_info{
        .sampler = *texture.getSampler(),
        .imageView = *texture.getImageView(),
        .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
    };

    vk::WriteDescriptorSet write_descriptor_set{
        .dstSet = *textureDescriptorSet_,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = vk::DescriptorType::eCombinedImageSampler,
        .pImageInfo = &descriptor_image_info
    };

    context_.getLogicalDevice().updateDescriptorSets({write_descriptor_set}, {});
}


DepthImage Renderer::createDepthResources(vk::Extent2D extent_2d)
{
    vk::Format depth_resource_format = context_.findDepthFormat();
    
    auto [depth_image, depth_image_memory] = createImage(
        extent_2d.width, 
        extent_2d.height, 
        1,
        context_.getMsaaSamples(),
        depth_resource_format,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eDepthStencilAttachment,
        vk::MemoryPropertyFlagBits::eDeviceLocal
    );

    transitionImageLayout(
        *depth_image, 
        vk::ImageLayout::eUndefined, 
        vk::ImageLayout::eDepthAttachmentOptimal,
        vk::ImageAspectFlagBits::eDepth,
        1
    );

    vk::raii::ImageView image_view = createImageView(
        *depth_image, 
        depth_resource_format, 
        vk::ImageAspectFlagBits::eDepth,
        1
    );

    return DepthImage(
        std::move(depth_image),
        std::move(depth_image_memory),
        std::move(image_view)
    );
}


MsaaColorImage Renderer::createMsaaColorImage(vk::Extent2D extent_2d, vk::Format format)
{
    auto [msaa_color_image, msaa_color_image_memory] = createImage(
        extent_2d.width,
        extent_2d.height,
        1,
        context_.getMsaaSamples(),
        format,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eTransientAttachment | vk::ImageUsageFlagBits::eColorAttachment,
        vk::MemoryPropertyFlagBits::eDeviceLocal
    );

    vk::raii::ImageView image_view = createImageView(
        *msaa_color_image,
        format,
        vk::ImageAspectFlagBits::eColor,
        1
    );

    return MsaaColorImage(
        std::move(msaa_color_image),
        std::move(msaa_color_image_memory),
        std::move(image_view)
    );
}


void Renderer::generateMipmaps(
        vk::Image image,
        vk::Format format, 
        uint32_t width,
        uint32_t height,
        uint32_t mip_levels
)
{
    vk::FormatProperties format_properties = context_.getPhysicalDevice().getFormatProperties(format);
    if (!(format_properties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImageFilterLinear))
    {
        throw std::runtime_error("Texture image format does not support blitting");
    }

    vk::raii::CommandBuffer command_buffer = beginSingleTimeCommands();
    
    vk::ImageMemoryBarrier image_memory_barrier = {
        .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
        .dstAccessMask = vk::AccessFlagBits::eTransferRead,
        .oldLayout = vk::ImageLayout::eTransferDstOptimal,
        .newLayout = vk::ImageLayout::eTransferSrcOptimal,
        .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
        .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
        .image = image
    };

    image_memory_barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    image_memory_barrier.subresourceRange.baseArrayLayer = 0;
	image_memory_barrier.subresourceRange.layerCount = 1;
    image_memory_barrier.subresourceRange.levelCount = 1;
    
    for (uint32_t i=1; i < mip_levels; i++)
    {
        // setting the correct index in base zero 
        image_memory_barrier.subresourceRange.baseMipLevel = i - 1;  // mip level 0 is the first mip level... why not use 0 from the begining???
        // resetting old and new layouts
        image_memory_barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
		image_memory_barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
        // reseting access mask 
        image_memory_barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
		image_memory_barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;

        command_buffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer, 
            vk::PipelineStageFlagBits::eTransfer, 
            {}, 
            {}, 
            {}, 
            image_memory_barrier
        );

        vk::ArrayWrapper1D<vk::Offset3D, 2> offsets; 
        vk::ArrayWrapper1D<vk::Offset3D, 2> dstOffsets;
        offsets[0]          = vk::Offset3D(0, 0, 0);
        offsets[1]          = vk::Offset3D(width, height, 1);
        dstOffsets[0]       = vk::Offset3D(0, 0, 0);
        dstOffsets[1]       = vk::Offset3D(width > 1 ? width / 2 : 1, height > 1 ? height / 2 : 1, 1);
        
        vk::ImageBlit blit{
            .srcSubresource = {},
            .srcOffsets = offsets,
            .dstSubresource = {},
            .dstOffsets = dstOffsets
        };

        blit.srcSubresource = vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, i - 1, 0, 1);
        blit.dstSubresource = vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, i, 0, 1);

        command_buffer.blitImage(
            image, 
            vk::ImageLayout::eTransferSrcOptimal,
            image, vk::ImageLayout::eTransferDstOptimal,
            {blit},
            vk::Filter::eLinear
        );

        image_memory_barrier.oldLayout     = vk::ImageLayout::eTransferSrcOptimal;
        image_memory_barrier.newLayout     = vk::ImageLayout::eShaderReadOnlyOptimal;
        image_memory_barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
        image_memory_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        command_buffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer, 
            vk::PipelineStageFlagBits::eFragmentShader,
            {},
            {},
            {},
            image_memory_barrier
        );

        if (width > 1)
            width /= 2;
        if (height > 1)
            height /= 2;
    }

    image_memory_barrier.subresourceRange.baseMipLevel = mip_levels - 1;
	image_memory_barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
	image_memory_barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
	image_memory_barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
	image_memory_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

	command_buffer.pipelineBarrier(
        vk::PipelineStageFlagBits::eTransfer,
        vk::PipelineStageFlagBits::eFragmentShader,
        {},
        {},
        {},
        image_memory_barrier
    );

	endSingleTimeCommands(std::move(command_buffer));

}


 void Renderer::createParticleSystem(const std::vector<Particle>& particles)
 {
    particleCount_ = static_cast<uint32_t>(particles.size());
    uint32_t particle_buffer_size = sizeof(Particle) * particleCount_;

    computeFrameSlots_.resize(MAX_FRAMES_IN_FLIGHT);

    for (auto& compute_frame_slot : computeFrameSlots_)
    {
        auto [particles_buffer, particles_buffer_memory] = uploadBufferToDevice(
            particles.data(), 
            particle_buffer_size, 
            vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eStorageBuffer
        );

        compute_frame_slot.particleBuffer_ = std::move(particles_buffer);
        compute_frame_slot.particleBufferMemory_ = std::move(particles_buffer_memory);
    }

    initializeComputeFrameSlots();    
 }


 void Renderer::initializeComputeFrameSlots()
 {
    for (uint32_t i=0; i<MAX_FRAMES_IN_FLIGHT; i++) 
    {
        vk::CommandBufferAllocateInfo command_buffer_allocate_info{
            .commandPool = *commandPool_,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1
        };

        std::vector<vk::raii::CommandBuffer> buffers = context_.getLogicalDevice().allocateCommandBuffers(command_buffer_allocate_info);
        computeFrameSlots_[i].computeCommandBuffer_ = std::move(buffers[0]); // pointing to the first commandBuffer from the command count.
        
        vk::FenceCreateInfo fence_create_info{
            .flags = vk::FenceCreateFlagBits::eSignaled
        };
        computeFrameSlots_[i].computeInflightFence_ = vk::raii::Fence(
            context_.getLogicalDevice(),
            fence_create_info
        );

        // refer to CreteSyncObjects() in the vulkan tutorial.
        computeFrameSlots_[i].computeFinishedSemaphore_ = vk::raii::Semaphore(context_.getLogicalDevice(), vk::SemaphoreCreateInfo());
        

        auto [compute_uniform_buffer, compute_uniform_buffer_memory] = createBuffer(
            sizeof(ComputeUniformBufferObject), 
            vk::BufferUsageFlagBits::eUniformBuffer,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
        );

        // initializing frame slot uniform buffer
        computeFrameSlots_[i].computeUniformBuffer_ = std::move(compute_uniform_buffer);
        computeFrameSlots_[i].computeUniformBufferMemory_ = std::move(compute_uniform_buffer_memory);
        computeFrameSlots_[i].computeUniformBufferMemoryMapped_ = computeFrameSlots_[i].computeUniformBufferMemory_.mapMemory(0, sizeof(ComputeUniformBufferObject));

        // allocating descriptor set
        vk::DescriptorSetAllocateInfo descriptor_set_allocate_info{
            .descriptorPool = *computeDescriptorPool_,
            .descriptorSetCount = 1,
            .pSetLayouts = &*(particleDescriptorLayout_.getLayout())
        };

        std::vector<vk::raii::DescriptorSet> description_sets = context_.getLogicalDevice().allocateDescriptorSets(descriptor_set_allocate_info);
        
        computeFrameSlots_[i].descriptorSet_ = std::move(description_sets.front());

        vk::DescriptorBufferInfo descriptor_buffer_info_0{
            .buffer = *computeFrameSlots_[i].computeUniformBuffer_,
            .offset = 0,
            .range = sizeof(ComputeUniformBufferObject)
        };

        vk::DescriptorBufferInfo descriptor_buffer_info_1{
            .buffer = *computeFrameSlots_[(i + MAX_FRAMES_IN_FLIGHT - 1)% MAX_FRAMES_IN_FLIGHT].particleBuffer_,
            .offset = 0,
            .range = sizeof(Particle) * particleCount_
        };

        vk::DescriptorBufferInfo descriptor_buffer_info_2{
            .buffer = *computeFrameSlots_[i].particleBuffer_,
            .offset = 0,
            .range = sizeof(Particle) *particleCount_
        };

        vk::WriteDescriptorSet write_descriptor_set_0{
            .dstSet = *computeFrameSlots_[i].descriptorSet_,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eUniformBuffer,
            .pBufferInfo = &descriptor_buffer_info_0
        };

        vk::WriteDescriptorSet write_descriptor_set_1{
            .dstSet = *computeFrameSlots_[i].descriptorSet_,
            .dstBinding = 1,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eStorageBuffer,
            .pBufferInfo = &descriptor_buffer_info_1
        };

        vk::WriteDescriptorSet write_descriptor_set_2{
            .dstSet = *computeFrameSlots_[i].descriptorSet_,
            .dstBinding = 2,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eStorageBuffer,
            .pBufferInfo = &descriptor_buffer_info_2
        };


        context_.getLogicalDevice().updateDescriptorSets(
            {write_descriptor_set_0, write_descriptor_set_1, write_descriptor_set_2}, 
            {}
        );
    }
 }