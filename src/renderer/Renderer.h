#pragma once

#include <vulkan/vulkan_raii.hpp>
#include <array>

#include "RenderFrameSlot.h"
#include "buffers/Vertex.h"
#include "buffers/Mesh.h"
#include "buffers/UniformBufferObject.h"

// Forward Declarations
class VulkanContext;
class SwapChain;
class GraphicsPipeline;
class FrameDescriptorLayout;



class Renderer
{
public:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

    Renderer(VulkanContext& context, const FrameDescriptorLayout& frame_descriptor_layout);

    // Non Copyable (removing copyable constructors)
    Renderer(const Renderer&) = delete;
    Renderer& operator =(const Renderer&) = delete;

    bool drawFrame(
        SwapChain& swap_chain, 
        GraphicsPipeline& graphics_pipeline, 
        const Mesh& mesh,
        const UniformBufferObject& uniform_buffer_object
    );
    
    Mesh createMesh(const std::vector<Vertex>& vertices, const std::vector<uint16_t>& indices);
    void initializePerImageResources(uint32_t image_count);

private:
    void createCommandPool();
    void createDescriptorPool();
    void initializeFrameData();
    void createFinishedSemaphores(uint32_t image_count);
    
    uint32_t findMemoryType(uint32_t type_filter, vk::MemoryPropertyFlags properties) const;
    
    auto createBuffer(
        vk::DeviceSize size, 
        vk::BufferUsageFlags usage, 
        vk::MemoryPropertyFlags memory_properties
    ) -> std::pair<vk::raii::Buffer, vk::raii::DeviceMemory>;

    void copyBuffer(
        vk::raii::Buffer &source_buffer, 
        vk::raii::Buffer &destination_buffer, 
        vk::DeviceSize size
    );

    auto uploadBufferToDevice(
        const void* data,
        vk::DeviceSize buffer_size, 
        vk::BufferUsageFlags usage
    ) -> std::pair<vk::raii::Buffer, vk::raii::DeviceMemory>;

    // private member variables
    VulkanContext& context_;
    
    vk::raii::CommandPool commandPool_ = nullptr;
    vk::raii::DescriptorPool descriptorPool_ = nullptr;
    
    const FrameDescriptorLayout& frameDescriptorLayout_;
    std::array<RenderFrameSlot, MAX_FRAMES_IN_FLIGHT> renderFrameSlots_;
    
    uint32_t currentFrame_ = 0;
    std::vector<vk::raii::Semaphore> renderFinishedSemaphores_;

};
