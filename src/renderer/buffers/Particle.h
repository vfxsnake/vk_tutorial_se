#pragma once

#include <vulkan/vulkan_raii.hpp>
#include <glm/glm.hpp>
#include <array>
#include <cstddef>


struct Particle
{
    glm::vec2 position_;
    glm::vec2 velocity_;
    glm::vec4 color_;

    static vk::VertexInputBindingDescription getBindingDescription()
    {
        vk::VertexInputBindingDescription binding_description{
            .binding = 0,
            .stride = sizeof(Particle),
            .inputRate = vk::VertexInputRate::eVertex
        };
        return binding_description;
    }

    static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescriptions()
    {
        vk::VertexInputAttributeDescription position_description{
            .location = 0,
            .binding = 0,
            .format = vk::Format::eR32G32Sfloat,
            .offset = offsetof(Particle, position_)
        };

        vk::VertexInputAttributeDescription color_description{
            .location = 1,
            .binding = 0,
            .format = vk::Format::eR32G32B32A32Sfloat,
            .offset = offsetof(Particle, color_)
        };

        return {position_description, color_description};

    }
};