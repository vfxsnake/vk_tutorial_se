#pragma once

#include <vulkan/vulkan_raii.hpp>
#include <array>
#include <vector>
#include <span>

#include "RenderFrameSlot.h"
#include "buffers/Vertex.h"
#include "buffers/Mesh.h"
#include "image_resources/Texture.h"
#include "image_resources/DepthImage.h"
#include "image_resources/MsaaColorImage.h"
#include "ComputeFrameSlot.h"
#include "buffers/Particle.h"
#include "ObjectRenderData.h"

// Forward Declarations
class VulkanContext;
class SwapChain;
class GraphicsPipeline;
class FrameDescriptorLayout;
class TextureDescriptorLayout;
class ComputePipeline;
class ParticleGraphicsPipeline;
class ParticleDescriptorLayout;



class Renderer
{
public:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;
    static constexpr uint32_t MAX_NUMBER_OF_OBJECTS = 16;

    Renderer(
        VulkanContext& context,
        const TextureDescriptorLayout& texture_descriptor_layout,
        const FrameDescriptorLayout& frame_descriptor_layout,
        const ComputePipeline& compute_pipeline,
        const ParticleGraphicsPipeline& particle_graphics_pipeline,
        const ParticleDescriptorLayout& particle_descriptor_layout
    );

    // Non Copyable (removing copyable constructors)
    Renderer(const Renderer&) = delete;
    Renderer& operator =(const Renderer&) = delete;

    bool drawFrame(
        SwapChain& swap_chain, 
        GraphicsPipeline& graphics_pipeline, 
        std::span<const UniformBufferObject> uniform_buffer_objects,
        vk::ImageView depth_image_view,
        vk::ImageView msaa_color_image_view,
        float delta_time
    );
    
    Mesh createMesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
    
    auto addMesh(Mesh&& mesh) -> uint32_t;

    void createObjectRenderData(uint32_t mesh_index);
    
    void initializePerImageResources(uint32_t image_count);

    Texture createTexture(const std::string& path);
    
    void bindTextureToDescriptor(const Texture& texture);

    auto createDepthResources(vk::Extent2D extent_2d) -> DepthImage;

    auto createMsaaColorImage(vk::Extent2D extent_2d, vk::Format format) -> MsaaColorImage;

    void generateMipmaps(
        vk::Image image,
        vk::Format format, 
        uint32_t width,
        uint32_t height,
        uint32_t mip_levels
    );

    void createParticleSystem(const std::vector<Particle>& particles);

private:
    void createCommandPool();
    void createDescriptorPool();
    void createTextureDescriptorPool();
    void initializeFrameData();
    void createFinishedSemaphores(uint32_t image_count);

    void createComputeDescriptorPool();
    void initializeComputeFrameSlots();
    
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
        uint32_t mip_levels,
        vk::SampleCountFlagBits num_samples,
        vk::Format format, 
        vk::ImageTiling tiling,
        vk::ImageUsageFlags usage_flags,
        vk::MemoryPropertyFlags memory_property_flags
    ) -> std::pair<vk::raii::Image, vk::raii::DeviceMemory>;

    auto createImageView(
        vk::Image image, 
        vk::Format format, 
        vk::ImageAspectFlags aspect_flags,
        uint32_t level_count
    ) -> vk::raii::ImageView;

    void transitionImageLayout(
        vk::Image image,
        vk::ImageLayout old_layout,
        vk::ImageLayout new_layout,
        vk::ImageAspectFlags aspect_flags,
        uint32_t level_count
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
    std::vector<Mesh> meshes_;
    std::vector<ObjectRenderData> objectRenderDataEntries_;
    std::vector<uint32_t> objectMeshIndices_;
    std::array<RenderFrameSlot, MAX_FRAMES_IN_FLIGHT> renderFrameSlots_;
    
    uint32_t currentFrame_ = 0;
    std::vector<vk::raii::Semaphore> renderFinishedSemaphores_;

    const ComputePipeline& computePipeline_;
    const ParticleGraphicsPipeline& particleGraphicsPipeline_;
    const ParticleDescriptorLayout& particleDescriptorLayout_;
    vk::raii::DescriptorPool computeDescriptorPool_ = nullptr;
    std::vector<ComputeFrameSlot> computeFrameSlots_;
    uint32_t particleCount_ = 0; 

};
