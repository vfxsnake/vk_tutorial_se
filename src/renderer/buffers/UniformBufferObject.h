#pragma once

#include <glm/glm.hpp>

struct UniformBufferObject
{
    glm::mat4 modelMatrix_;
    glm::mat4 viewMatrix_;
    glm::mat4 projectionMatrix_;
};