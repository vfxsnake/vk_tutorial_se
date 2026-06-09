#pragma once 

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace scene
{

    struct Transform
    {
        glm::vec3 position = {0.0f, 0.0f, 0.0f};
        glm::vec3 rotation = {0.0f, 0.0f, 0.0f}; // euler angles (radians)
        glm::vec3 scale = {1.0f, 1.0f, 1.0f};
        
        auto getModelMatrix() const -> glm::mat4
        {
            glm::mat4 model_matrix =  glm::mat4(1.0f);
            model_matrix = glm::translate(model_matrix, position);
            model_matrix = glm::rotate(model_matrix, rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
            model_matrix = glm::rotate(model_matrix, rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
            model_matrix = glm::rotate(model_matrix, rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
            model_matrix = glm::scale(model_matrix, scale);
            return model_matrix;
        }
    };

} // end of scene name space