#pragma once

#include <vulkan/vulkan_raii.hpp>
#include <array>

#include "RenderFrameSlot.h"
#include "buffers/Vertex.h"
#include "buffers/Mesh.h"

// Forward Declarations
class VulkanContext;
class SwapChain;
class GraphicsPipeline;


class Renderer
{
public:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

    Renderer(VulkanContext& context);

    // Non Copyable (removing copyable constructors)
    Renderer(const Renderer&) = delete;
    Renderer& operator =(const Renderer&) = delete;

    bool drawFrame(SwapChain& swap_chain, GraphicsPipeline& graphics_pipeline, const Mesh& mesh);
    Mesh createMesh(const std::vector<Vertex>& vertices);

private:
    void createCommandPool();
    void initializeFrameData();
    
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

    // private member variables
    VulkanContext& context_;
    vk::raii::CommandPool commandPool_ = nullptr;
    std::array<RenderFrameSlot, MAX_FRAMES_IN_FLIGHT> renderFrameSlots_;
    uint32_t currentFrame_ = 0;
    
};