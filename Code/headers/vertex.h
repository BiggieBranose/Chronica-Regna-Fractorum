#pragma once

namespace CRF{
    struct vec2{
        float x, y;
    };

    struct vec3{
        float x, y, z;
    };

    struct vertex{
        vec3 position;
        vec2 texcoord; 
        vec3 normal;
    };
}