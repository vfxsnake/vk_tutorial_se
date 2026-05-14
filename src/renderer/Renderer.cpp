#include "Renderer.h"
#include "core/VulkanContext.h"
#include "core/SwapChain.h"
#include "GraphicsPipeline.h"
#include "descriptors/FrameDescriptorLayout.h"
#include "descriptors/TextureDescriptorLayout.h"

#include <stdexcept>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

Renderer::Renderer(
    VulkanContext& context, 
    const FrameDescriptorLayout& frame_descriptor_layout,
    const TextureDescriptorLayout& texture_descriptor_layout
) : context_(context), 
    frameDescriptorLayout_(frame_descriptor_layout),
    textureDescriptorLayout_(texture_descriptor_layout)
{
    createCommandPool();
    createDescriptorPool();
    createTextureDescriptorPool();
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


void Renderer::createDescriptorPool()
{
    vk::DescriptorPoolSize descriptor_pool_size{
        .type = vk::DescriptorType::eUniformBuffer,
        .descriptorCount = MAX_FRAMES_IN_FLIGHT
    };

    vk::DescriptorPoolCreateInfo descriptor_pool_create_info{
        .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, 
        .maxSets = MAX_FRAMES_IN_FLIGHT,
        .poolSizeCount = 1,
        .pPoolSizes = &descriptor_pool_size  
    };

    descriptorPool_ = vk::raii::DescriptorPool(context_.getLogicalDevice(), descriptor_pool_create_info);
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

        auto [uniform_buffer, uniform_buffer_memory] = createBuffer(
            sizeof(UniformBufferObject), 
            vk::BufferUsageFlagBits::eUniformBuffer,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
        );

        // initializing frame slot uniform buffer
        render_frame_slot.uniformBuffer_ = std::move(uniform_buffer);
        render_frame_slot.uniformBufferMemory_ = std::move(uniform_buffer_memory);
        render_frame_slot.uniformBufferMappedMemory_ = render_frame_slot.uniformBufferMemory_.mapMemory(0, sizeof(UniformBufferObject)) ;

        // allocating descriptor set
        vk::DescriptorSetAllocateInfo descriptor_set_allocate_info{
            .descriptorPool = descriptorPool_,
            .descriptorSetCount = 1,
            .pSetLayouts = &*(frameDescriptorLayout_.getLayout()),
        };

        std::vector<vk::raii::DescriptorSet> description_sets = context_.getLogicalDevice().allocateDescriptorSets(descriptor_set_allocate_info);
        render_frame_slot.descriptorSet_ = std::move(description_sets.front());

        vk::DescriptorBufferInfo descriptor_buffer_info{
            .buffer = *render_frame_slot.uniformBuffer_,
            .offset = 0,
            .range = sizeof(UniformBufferObject)
        };

        vk::WriteDescriptorSet write_descriptor_set{
            .dstSet = *render_frame_slot.descriptorSet_,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eUniformBuffer,
            .pBufferInfo = &descriptor_buffer_info
        };

        context_.getLogicalDevice().updateDescriptorSets({write_descriptor_set}, {});
    }
}


bool Renderer::drawFrame(
    SwapChain& swap_chain, 
    GraphicsPipeline& graphics_pipeline, 
    const Mesh& mesh, 
    const UniformBufferObject& uniform_buffer_object
)
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

    // updating uniform buffer
    renderFrameSlots_[currentFrame_].updateUniformBuffer(uniform_buffer_object);
    
    // reseting the command buffer
    renderFrameSlots_[currentFrame_].commandBuffer_.reset();
    
    // recording the command buffer using the graphics_pipeline record command
    graphics_pipeline.record(
        renderFrameSlots_[currentFrame_].commandBuffer_,
        renderFrameSlots_[currentFrame_].descriptorSet_,
        textureDescriptorSet_,
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
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = vk::SampleCountFlagBits::e1,
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


vk::raii::ImageView Renderer::createImageView(vk::Image image, vk::Format format)
{
    vk::ImageViewCreateInfo image_view_create_info{
        .image = image,
        .viewType = vk::ImageViewType::e2D,
        .format = format,
        .subresourceRange = {
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    return vk::raii::ImageView(context_.getLogicalDevice(), image_view_create_info);
} 


void Renderer::transitionImageLayout(
        vk::Image image,
        vk::ImageLayout old_layout,
        vk::ImageLayout new_layout
    )
{
    vk::ImageMemoryBarrier image_memory_barrier{
        .oldLayout = old_layout,
        .newLayout = new_layout,
        .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
        .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
        .image = image,
        .subresourceRange = {
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel = 0,
            .levelCount = 1,
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
        .compareOp = vk::CompareOp::eAlways
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
        vk::Format::eR8G8B8A8Srgb,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
        vk::MemoryPropertyFlagBits::eDeviceLocal
    );

    transitionImageLayout(
        *texture_image, 
        vk::ImageLayout::eUndefined, 
        vk::ImageLayout::eTransferDstOptimal
    );
    
    copyBufferToImage(
        *staging_buffer, 
        *texture_image, 
        static_cast<uint32_t>(texture_width), static_cast<uint32_t>(texture_height)
    );
    
    transitionImageLayout(
        *texture_image, 
        vk::ImageLayout::eTransferDstOptimal, 
        vk::ImageLayout::eShaderReadOnlyOptimal
    );
    
    vk::raii::ImageView image_view = createImageView(*texture_image, vk::Format::eR8G8B8A8Srgb);
    
    vk::raii::Sampler sampler = createSampler(); 
    
    return Texture(
        std::move(texture_image),
        std::move(texture_image_memory),
        std::move(image_view),
        std::move(sampler)
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

