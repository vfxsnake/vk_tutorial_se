#include "ComputeThreadPool.h"

#include "core/VulkanContext.h"

ComputeThreadPool::ComputeThreadPool(
    const VulkanContext& context,
    const ComputePipeline& compute_pipeline,
    const ParticleDescriptorLayout& particle_descriptor_layout,
    uint32_t thread_count
) : context_(context), 
    computePipeline_(compute_pipeline),
    particleDescriptorLayout_(particle_descriptor_layout),
    threadCount_(thread_count)
{
    // using resize to populate all vector member variables.
    commandPools_.resize(threadCount_);
    commandBuffers_.resize(threadCount_);
    fences_.resize(threadCount_);
    workReady_.resize(threadCount_);
    workDone_.resize(threadCount_);
    currentDescriptorSets_.resize(threadCount_);
    workerThreads_.resize(threadCount_);
    
    createCommandPoolsAndBuffers();
    createFences();
    
    for (uint32_t i = 0; i < threadCount_; i++)
    {
        workReady_[i] = false;
        workDone_[i] = false;
        workerThreads_[i] = std::thread(
            &ComputeThreadPool::workerThreadFunction, // reference of the function
            this, // object that holds that function
            i // argument of the function if any, add more entries as arguments are needed.
        );
    }
}


ComputeThreadPool::~ComputeThreadPool()
{
    shouldExit_ = true;
    for (uint32_t i = 0; i < threadCount_; i++)
    {
        workReady_[i] = true;
    }

    for (auto& current_thread: workerThreads_)
    {
        if (current_thread.joinable())
        {
            current_thread.join();
        }
    }
}


void ComputeThreadPool::createCommandPoolsAndBuffers()
{
    for (uint32_t i = 0; i < threadCount_; i++)
    {
        vk::CommandPoolCreateInfo command_pool_create_info{
            .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            .queueFamilyIndex = context_.getQueueFamilyIndex()
        };

        // direct construct of command pool to avoid vk::raii::commandPool twice.
        vk::raii::CommandPool command_pool(
            context_.getLogicalDevice(), 
            command_pool_create_info
        );
        
        vk::CommandBufferAllocateInfo command_buffer_allocate_info{
            .commandPool = *command_pool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1
        };
        
        std::vector<vk::raii::CommandBuffer> buffers = context_.getLogicalDevice().allocateCommandBuffers(
            command_buffer_allocate_info
        );
        
        commandPools_[i] = std::move(command_pool);
        commandBuffers_[i] = std::move(buffers[0]);
    }
}


void ComputeThreadPool::createFences()
{
    for (uint32_t i = 0; i < threadCount_; i++)
    {
        vk::FenceCreateInfo fence_create_info{
            .flags = vk::FenceCreateFlagBits::eSignaled
        };
        
        fences_[i] = vk::raii::Fence(
            context_.getLogicalDevice(),
            fence_create_info
        );
    }
}


void ComputeThreadPool::workerThreadFunction(uint32_t thread_index)
{
    while (true)
    {
        workReady_[thread_index].wait(false); // waits until it becomes false or wait for false.
        workReady_[thread_index].store(false); // resets the value for next dispatch.
        if (shouldExit_)
        {
            break;
        }

        context_.getLogicalDevice().waitForFences(*fences_[thread_index], vk::True, UINT64_MAX);
        context_.getLogicalDevice().resetFences(*fences_[thread_index]);

        commandBuffers_[thread_index].reset();
        vk::CommandBufferBeginInfo command_buffer_begin_info{
            .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
        };
        commandBuffers_[thread_index].begin(command_buffer_begin_info);

        commandBuffers_[thread_index].bindPipeline(
            vk::PipelineBindPoint::eCompute, 
            *computePipeline_.getPipeline()
        );

        commandBuffers_[thread_index].bindDescriptorSets(
            vk::PipelineBindPoint::eCompute,
            *computePipeline_.getPipelineLayout(),
            0,
            currentDescriptorSets_[thread_index],
            {}
        );
    }
}