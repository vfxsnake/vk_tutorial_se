#pragma once

#include <vulkan/vulkan_raii.hpp>

class Mesh{
public:
    
    Mesh(
        vk::raii::Buffer  vertex_buffer,
        vk::raii::DeviceMemory vertex_memory,
        uint32_t vertex_count,
        vk::raii::Buffer index_buffer,
        vk::raii::DeviceMemory index_memory,
        uint32_t index_count,
        vk::IndexType index_type
    );
    
    // deleting copy constructures
    Mesh(const Mesh&) = delete;
    Mesh& operator =(const Mesh&) = delete;

    // assigning move constructures as default
    Mesh(Mesh&&) = default;
    Mesh& operator =(Mesh&&) = default;

    // accessor functions
    uint32_t getVertexCount() const;
    uint32_t getIndexCount() const;
    
    void bind(vk::CommandBuffer command_buffer) const;


private:
    vk::raii::Buffer vertexBuffer_;
    vk::raii::DeviceMemory vertexMemory_;
    uint32_t vertexCount_;
    
    vk::raii::Buffer indexBuffer_;
    vk::raii::DeviceMemory indexMemory_;
    uint32_t indexCount_;
    vk::IndexType indexType_;
};