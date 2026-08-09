#pragma once

#include "core/Types.hpp"
#include <string>
#include <vector>

namespace crf {

struct ImageData {
    u32 width = 0;
    u32 height = 0;
    std::vector<unsigned char> pixels;
};

struct PrimitiveData {
    u32 firstIndex = 0;
    u32 indexCount = 0;
    u32 textureIndex = 0;
};

struct MeshData {
    std::vector<float> positions;
    std::vector<float> texCoords;
    std::vector<unsigned int> indices;
    std::vector<ImageData> images;
    std::vector<PrimitiveData> primitives;
    std::vector<float> normals;
};

const MeshData& loadScene(const std::string& filepath);

}