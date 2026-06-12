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