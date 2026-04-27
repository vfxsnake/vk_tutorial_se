#include "Mesh.h"


Mesh::Mesh(
    vk::raii::Buffer  vertex_buffer,
    vk::raii::DeviceMemory vertex_memory,
    uint32_t vertex_count,
    vk::raii::Buffer index_buffer,
    vk::raii::DeviceMemory index_memory,
    uint32_t index_count,
    vk::IndexType index_type
) : // initialization list
    vertexBuffer_(std::move(vertex_buffer)), 
    vertexMemory_(std::move(vertex_memory)), 
    vertexCount_(vertex_count),
    indexBuffer_(std::move(index_buffer)),
    indexMemory_(std::move(index_memory)),
    indexCount_(index_count),
    indexType_(index_type)
{ }


uint32_t Mesh::getVertexCount() const
{
    return vertexCount_;
}


uint32_t Mesh::getIndexCount() const
{
    return indexCount_;
}


void Mesh::bind(vk::CommandBuffer command_buffer) const
{
    command_buffer.bindVertexBuffers(0, *vertexBuffer_, {0});
    command_buffer.bindIndexBuffer(*indexBuffer_, 0, indexType_);
}
