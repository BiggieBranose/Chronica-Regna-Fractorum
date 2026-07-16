#include "ModelLoader.hpp"
#include "core/Log.hpp"
#include "core/Assert.hpp"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <unordered_map>

namespace crf {

size_t VertexHash::operator()(const Vertex& vertex) const {
    size_t h = 0;
    auto hashCombine = [&h](auto val) {
        h ^= std::hash<decltype(val)>{}(val) + 0x9e3779b9 + (h << 6) + (h >> 2);
    };

    for (int i = 0; i < 3; i++) {
        hashCombine(vertex.pos[i]);
    }
    for (int i = 0; i < 3; i++) {
        hashCombine(vertex.color[i]);
    }
    for (int i = 0; i < 2; i++) {
        hashCombine(vertex.texCoord[i]);
    }

    return h;
}

void ModelLoader::loadModel(const std::string& filepath,
                            std::vector<Vertex>& vertices,
                            std::vector<u32>& indices) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filepath.c_str())) {
        Log::error("Failed to load model: {} {}", warn, err);
        return;
    }

    std::unordered_map<Vertex, u32, VertexHash> uniqueVertices{};

    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            Vertex vertex{};

            vertex.pos[0] = attrib.vertices[3 * index.vertex_index + 0];
            vertex.pos[1] = attrib.vertices[3 * index.vertex_index + 1];
            vertex.pos[2] = attrib.vertices[3 * index.vertex_index + 2];

            vertex.color[0] = attrib.colors[3 * index.vertex_index + 0];
            vertex.color[1] = attrib.colors[3 * index.vertex_index + 1];
            vertex.color[2] = attrib.colors[3 * index.vertex_index + 2];

            if (index.texcoord_index >= 0) {
                vertex.texCoord[0] = attrib.texcoords[2 * index.texcoord_index + 0];
                vertex.texCoord[1] = 1.0f - attrib.texcoords[2 * index.texcoord_index + 1];
            }

            if (uniqueVertices.count(vertex) == 0) {
                uniqueVertices[vertex] = static_cast<u32>(vertices.size());
                vertices.push_back(vertex);
            }

            indices.push_back(uniqueVertices[vertex]);
        }
    }

    Log::info("Loaded model: {} vertices, {} indices", vertices.size(), indices.size());
}

}
