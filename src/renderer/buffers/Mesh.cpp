#include "Mesh.h"


Mesh::Mesh(
    vk::raii::Buffer  vertex_buffer,
    vk::raii::DeviceMemory vertex_memory,
    uint32_t vertex_count
) : // initialization list
    vertexBuffer_(std::move(vertex_buffer)), 
    vertexMemory_(std::move(vertex_memory)), 
    vertexCount_(vertex_count) 
{ }


const vk::raii::Buffer& Mesh::getVertexBuffer() const
{
    return vertexBuffer_;
}
    

uint32_t Mesh::getVertexCount() const
{
    return vertexCount_;
}
