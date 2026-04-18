#pragma once

#include <vulkan/vulkan_raii.hpp>

class Mesh{
public:
    
    Mesh(
        vk::raii::Buffer  vertex_buffer,
        vk::raii::DeviceMemory vertex_memory,
        uint32_t vertex_count
    );
    
    // deleting copy constructures
    Mesh(const Mesh&) = delete;
    Mesh& operator =(const Mesh&) = delete;

    // assigning move constructures as default
    Mesh(Mesh&&) = default;
    Mesh& operator =(Mesh&&) = default;

    // accessor functions
    auto getVertexBuffer() const -> const vk::raii::Buffer&;
    uint32_t getVertexCount() const; 


private:
    vk::raii::Buffer vertexBuffer_;
    vk::raii::DeviceMemory vertexMemory_;
    uint32_t vertexCount_;
};