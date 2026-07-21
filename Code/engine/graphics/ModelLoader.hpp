#pragma once

#include "core/Types.hpp"
#include "Vertex.hpp"
#include <vector>
#include <string>
#include <unordered_map>

namespace crf {

struct VertexHash {
    size_t operator()(const Vertex& vertex) const;
};

class ModelLoader {
public:
    static void loadModel(const std::string& filepath,
                          std::vector<Vertex>& vertices,
                          std::vector<u32>& indices);
};

}
