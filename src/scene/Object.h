#pragma once

#include "Transform.h"

namespace scene
{

    struct Object
    {
        Transform transform;
        uint32_t mesh_index = 0;  // ECS id
        uint32_t texture_index = 0; // future texture pool id
    };
} // end of scene namespace

