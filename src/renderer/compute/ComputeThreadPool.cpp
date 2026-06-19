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
    currentDescriptorSets_.resize(threadCount_);
    workerThreads_.resize(threadCount_);
    
    createCommandPoolsAndBuffers();
    
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
        workReady_[i].notify_one();
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
    for (uint32_t frame_in_flight = 0; frame_in_flight < MAX_FRAMES_IN_FLIGHT; frame_in_flight++ )
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
            
            commandPools_[frame_in_flight].push_back(std::move(command_pool));
            commandBuffers_[frame_in_flight].push_back(std::move(buffers[0]));
        }
    }
}


uint32_t ComputeThreadPool::getThreadCount() const
{
    return threadCount_;
}


void ComputeThreadPool::workerThreadFunction(uint32_t thread_index)
{
    while (true)
    {
         while (!workReady_[thread_index].load())
        {
            workReady_[thread_index].wait(false);
        }
        workReady_[thread_index].store(false);
        
        if (shouldExit_)
        {
            break;
        }

        commandBuffers_[currentFrameIndex_][thread_index].reset();
        vk::CommandBufferBeginInfo command_buffer_begin_info{
            .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
        };
        commandBuffers_[currentFrameIndex_][thread_index].begin(command_buffer_begin_info);

        commandBuffers_[currentFrameIndex_][thread_index].bindPipeline(
            vk::PipelineBindPoint::eCompute, 
            *computePipeline_.getPipeline()
        );

        commandBuffers_[currentFrameIndex_][thread_index].bindDescriptorSets(
            vk::PipelineBindPoint::eCompute,
            *computePipeline_.getPipelineLayout(),
            0,
            currentDescriptorSets_[thread_index],
            {}
        );

        // Compute push constants for this thread slice
        uint32_t particles_per_thread = currentParticleCount_ / threadCount_;
        uint32_t start_index_value = thread_index * particles_per_thread;
        uint32_t particle_count_to_process;
        if (thread_index == threadCount_ -1)
        {
            particle_count_to_process = currentParticleCount_ - start_index_value;
        }
        else
        {
            particle_count_to_process = particles_per_thread;
        }

        PushConstants push_constants{start_index_value, particle_count_to_process};

        commandBuffers_[currentFrameIndex_][thread_index].pushConstants<PushConstants>( // pushing constants type PushConstants struct
            *computePipeline_.getPipelineLayout(),
            vk::ShaderStageFlagBits::eCompute,
            0,
            push_constants // pasing the value to be taken as reference.
        );

        // dispatching
        commandBuffers_[currentFrameIndex_][thread_index].dispatch((particle_count_to_process + 255) / 256, 1, 1);
        commandBuffers_[currentFrameIndex_][thread_index].end();
        
        {
            std::lock_guard<std::mutex> done_lock(workDoneMutex_);
            workDone_[thread_index] = true;
        }
        workDoneConditionVariable_.notify_one();
    }
}


std::vector<vk::CommandBuffer> ComputeThreadPool::dispatch(
    std::span<const vk::raii::DescriptorSet*> compute_descriptor_sets,
    uint32_t particle_count,
    uint32_t current_frame
)
{
    currentParticleCount_ = particle_count;
    currentFrameIndex_ = current_frame;
    for (uint32_t i = 0; i < threadCount_; i++)
    {
        currentDescriptorSets_[i] = *compute_descriptor_sets[i];
    }

    for (uint32_t i = 0;  i < threadCount_; i++)
    {
        workDone_[i] = false;
        workReady_[i] = true;
        workReady_[i].notify_one();
    }

    std::unique_lock lock(workDoneMutex_);
    
    workDoneConditionVariable_.wait(
        lock, 
        [&]{
            return std::ranges::all_of(
                std::span(workDone_).first(threadCount_),
                [](const std::atomic<bool>& done)
                {
                    return done.load();
                }   
            );
        }
    );

    std::vector<vk::CommandBuffer> result;
    for (uint32_t i = 0; i < threadCount_; i++)
    {
        result.push_back(*commandBuffers_[currentFrameIndex_][i]);
    }

    return result;
}