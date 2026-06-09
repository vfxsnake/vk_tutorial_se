#pragma once

#include <vulkan/vulkan_raii.hpp>
#include <vector>
#include <cstring>

#include "buffers/UniformBufferObject.h"

struct ObjectRenderData
{
    std::vector<vk::raii::Buffer> uniformBuffers_;
    std::vector<vk::raii::DeviceMemory> uniformBufferMemories_;
    std::vector<void*> uniformBufferMemoriesMapped_;
    std::vector<vk::raii::DescriptorSet> descriptorSets_;

    ObjectRenderData() = default;
    
    // deleting copy constructors 
    ObjectRenderData(const ObjectRenderData&) = delete;
    ObjectRenderData& operator =(const ObjectRenderData&) = delete;
    
    // setting move constructors default
    ObjectRenderData(ObjectRenderData&&) = default;
    ObjectRenderData& operator =(ObjectRenderData&&) = default;

    void updateUniformBuffer(
        uint32_t frame_index,
        const UniformBufferObject& uniform_buffer_object
    )
    {
        memcpy(
            uniformBufferMemoriesMapped_[frame_index],
            &uniform_buffer_object,
            sizeof(UniformBufferObject)
        );
    }

};