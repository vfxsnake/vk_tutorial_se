#pragma once

#include <vulkan/vulkan_raii.hpp>
#include <array>

#include "RenderFrameSlot.h"
#include "buffers/Vertex.h"
#include "buffers/Mesh.h"
#include "buffers/UniformBufferObject.h"
#include "image_resources/Texture.h"

// Forward Declarations
class VulkanContext;
class SwapChain;
class GraphicsPipeline;
class FrameDescriptorLayout;
class TextureDescriptorLayout;



class Renderer
{
public:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

    Renderer(
        VulkanContext& context, 
        const FrameDescriptorLayout& frame_descriptor_layout,
        const TextureDescriptorLayout& texture_descriptor_layout
    );

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

    Texture createTexture(const std::string& path);
    void bindTextureToDescriptor(const Texture& texture);

private:
    void createCommandPool();
    void createDescriptorPool();
    void createTextureDescriptorPool();
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

    auto beginSingleTimeCommands() -> vk::raii::CommandBuffer;
    
    void endSingleTimeCommands(vk::raii::CommandBuffer command_buffer);

    auto createImage(
        uint32_t width, 
        uint32_t height, 
        vk::Format format, 
        vk::ImageTiling tiling,
        vk::ImageUsageFlags usage_flags,
        vk::MemoryPropertyFlags memory_property_flags
    ) -> std::pair<vk::raii::Image, vk::raii::DeviceMemory>;

    auto createImageView(vk::Image image, vk::Format format) -> vk::raii::ImageView;

    void transitionImageLayout(
        vk::Image image,
        vk::ImageLayout old_layout,
        vk::ImageLayout new_layout
    );

    void copyBufferToImage(
        vk::Buffer buffer,
        vk::Image image,
        uint32_t width,
        uint32_t height
    );

    auto createSampler() -> vk::raii::Sampler;

    // private member variables
    VulkanContext& context_;
    
    vk::raii::CommandPool commandPool_ = nullptr;
    const TextureDescriptorLayout& textureDescriptorLayout_;  
    vk::raii::DescriptorPool descriptorPool_ = nullptr;
    vk::raii::DescriptorPool textureDescriptorPool_ = nullptr;
    vk::raii::DescriptorSet textureDescriptorSet_ = nullptr;

    const FrameDescriptorLayout& frameDescriptorLayout_;
    std::array<RenderFrameSlot, MAX_FRAMES_IN_FLIGHT> renderFrameSlots_;
    
    uint32_t currentFrame_ = 0;
    std::vector<vk::raii::Semaphore> renderFinishedSemaphores_;

};
