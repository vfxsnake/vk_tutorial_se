#pragma once

#include <vector>
#include <string>
#include "renderer/buffers/Vertex.h"

struct ModelData
{
    std::vector<Vertex> vertices_;
    std::vector<uint32_t> indices_;
};


ModelData loadModel(const std::string& path);
