#include "ModelLoader.h"

#include <unordered_map>
#include <stdexcept>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

ModelData loadModel(const std::string& path)
{
    tinyobj::attrib_t attributes;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn;
    std::string err;

    if(!tinyobj::LoadObj(&attributes, &shapes, &materials, &warn, &err, path.c_str()))
    {
        throw std::runtime_error(warn + err);
    }

    std::unordered_map<Vertex, uint32_t> unique_vertices{};
    ModelData model_data;

    for (const auto& shape : shapes)
    {
        for (const auto& index : shape.mesh.indices)
        {
            Vertex vertex;
            vertex.pos = {
                attributes.vertices[(3 * index.vertex_index) + 0],
                attributes.vertices[(3 * index.vertex_index) + 1],
                attributes.vertices[(3 * index.vertex_index) + 2]
            };

            vertex.uv = {
                attributes.texcoords[(2* index.texcoord_index) + 0],
                1.0f - attributes.texcoords[(2* index.texcoord_index) + 1],
            };

            vertex.color = {1.0f, 1.0f, 1.0f};
            if (unique_vertices.count(vertex) == 0)
            {
                unique_vertices[vertex] = static_cast<uint32_t>(model_data.vertices_.size());
                model_data.vertices_.push_back(vertex);
            }
            model_data.indices_.push_back(unique_vertices[vertex]);

        }

    }

    return model_data;
}
    