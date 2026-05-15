#pragma once

#include <vulkan/vulkan_raii.hpp>
#include <glm/glm.hpp>
#include <array>
#include <cstddef>

struct Vertex
{
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 uv; // texture coordinates

    static vk::VertexInputBindingDescription getBindingDescription()
    {
        vk::VertexInputBindingDescription binding_description{
            .binding = 0,
            .stride = sizeof(Vertex),
            .inputRate = vk::VertexInputRate::eVertex
        };

        return binding_description;
    }

    static std::array<vk::VertexInputAttributeDescription, 3> getAttributeDescriptions()
    {
        vk::VertexInputAttributeDescription position_description{
            .location = 0,
            .binding = 0,
            .format = vk::Format::eR32G32B32Sfloat,
            .offset = offsetof(Vertex, pos)
        };

        vk::VertexInputAttributeDescription color_description{
            .location = 1,
            .binding = 0,
            .format = vk::Format::eR32G32B32Sfloat,
            .offset = offsetof(Vertex, color)
        };

        vk::VertexInputAttributeDescription uv_description{
            .location =2,
            .binding = 0,
            .format = vk::Format::eR32G32Sfloat,
            .offset = offsetof(Vertex, uv)
        };

        return {position_description, color_description, uv_description};
    }
};

