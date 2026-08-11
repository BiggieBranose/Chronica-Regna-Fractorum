#pragma once

#include "core/Types.hpp"
#include <string>
#include <vector>
#include <glm/glm.hpp>

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

struct NodeData {
    std::string name;
    glm::mat4 worldTransform{1.0f};
    u32 meshIndex = 0;
    u32 firstPrimitive = 0;
    u32 primitiveCount = 0;
    glm::vec3 aabbMin{0.0f};
    glm::vec3 aabbMax{0.0f};
};

struct MeshData {
    std::vector<float> positions;
    std::vector<float> texCoords;
    std::vector<unsigned int> indices;
    std::vector<ImageData> images;
    std::vector<PrimitiveData> primitives;
    std::vector<float> normals;
    std::vector<NodeData> nodes;
};

const MeshData& loadScene(const std::string& filepath);

}