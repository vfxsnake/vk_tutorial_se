#pragma once

#include <vulkan/vulkan_raii.hpp>
#include <cstring>
#include "compute/ComputeUniformBufferObject.h"

struct ComputeFrameSlot
{
    vk::raii::CommandBuffer computeCommandBuffer_ = nullptr;
    vk::raii::Fence computeInflightFence_ = nullptr;
    vk::raii::Semaphore computeFinishedSemaphore_ = nullptr;
    vk::raii::Buffer particleBuffer_ = nullptr;
    vk::raii::DeviceMemory particleBufferMemory_ = nullptr;

    // uniform buffers
    vk::raii::Buffer computeUniformBuffer_ = nullptr;
    vk::raii::DeviceMemory computeUniformBufferMemory_ = nullptr;
    void* computeUniformBufferMemoryMapped_ = nullptr;

    vk::raii::DescriptorSet descriptorSet_ = nullptr;

    ComputeFrameSlot() = default;    
    // deleting copy constructors
    ComputeFrameSlot(const ComputeFrameSlot&) = delete;
    ComputeFrameSlot& operator =(const ComputeFrameSlot&) = delete;

    ComputeFrameSlot(ComputeFrameSlot&&) = default;
    ComputeFrameSlot& operator=(ComputeFrameSlot&&) = default;

    
    void updateComputeUniformBuffer(const ComputeUniformBufferObject& compute_unform_buffer)
    {
        std::memcpy(computeUniformBufferMemoryMapped_, &compute_unform_buffer, sizeof(compute_unform_buffer));
    }
};