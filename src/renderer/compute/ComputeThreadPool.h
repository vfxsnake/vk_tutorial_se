#pragma once

#include <thread>
#include <vector>
#include <array>
#include <span>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <vulkan/vulkan_raii.hpp>

#include  "ComputePipeline.h"
#include "ParticleDescriptorLayout.h"


// forward declarations
class VulkanContext;


class ComputeThreadPool
{
public:
    ComputeThreadPool(
        const VulkanContext& context,
        const ComputePipeline& compute_pipeline,
        const ParticleDescriptorLayout& particle_descriptor_layout,
        uint32_t thread_count
    );

    ~ComputeThreadPool();

    // deleting copy operations
    ComputeThreadPool(const ComputeThreadPool&) = delete;
    ComputeThreadPool& operator =(const ComputeThreadPool&) = delete;

    auto dispatch(
        std::span<const vk::raii::DescriptorSet*> compute_descriptor_sets,
        uint32_t particle_count,
        uint32_t current_frame
    ) -> std::vector<vk::CommandBuffer>;

    // accessor function
    uint32_t getThreadCount() const;

private:

    void workerThreadFunction(uint32_t thread_index);
    void createCommandPoolsAndBuffers();

    const VulkanContext& context_;
    const ComputePipeline& computePipeline_;
    const ParticleDescriptorLayout& particleDescriptorLayout_;
    static constexpr uint32_t MAX_THREAD_COUNT = 16;
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;
    uint32_t threadCount_;

    // Per-thread GPU resource
    std::array<std::vector<vk::raii::CommandPool>, MAX_FRAMES_IN_FLIGHT> commandPools_;
    std::array<std::vector<vk::raii::CommandBuffer>, MAX_FRAMES_IN_FLIGHT> commandBuffers_;

    // Threading primitives
    std::vector<std::thread> workerThreads_;
    std::atomic<bool> shouldExit_{false};
    
    // using a capped array for simplicity for now. it remove complexity in the initialization of this variables.
    std::array<std::atomic<bool>, MAX_THREAD_COUNT> workReady_{};
    std::array<std::atomic<bool>, MAX_THREAD_COUNT> workDone_{};
    
    std::condition_variable workDoneConditionVariable_;
    std::mutex workDoneMutex_;

    // per-dispatch state set by main thread before signalling workers
    std::vector<vk::DescriptorSet> currentDescriptorSets_;
    uint32_t currentParticleCount_{0};
    uint32_t currentFrameIndex_{0};
};