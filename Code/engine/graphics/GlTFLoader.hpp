#pragma once

#include <string>
#include <vector>

namespace crf {

struct MeshData {
    std::vector<float> positions;
    std::vector<unsigned int> indices;
};

const MeshData& loadScene(const std::string& filepath);

}