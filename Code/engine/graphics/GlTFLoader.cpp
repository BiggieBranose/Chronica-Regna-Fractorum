#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_IMPLEMENTATION
#include "tinygltf/tiny_gltf.h"

#include "GlTFLoader.hpp"
#include <core/Log.hpp>

namespace crf {
    void loadScene(const std::string& filepath) {
        crf::Log::info("Loading glTF scene from: {}", filepath);
        tinygltf::Model model;
        tinygltf::TinyGLTF loader;
        std::string err;
        std::string warn;

        if (!loader.LoadBinaryFromFile(&model, &err, &warn, filepath)) {
            crf::Log::error("Failed to load glTF scene from: {}", filepath);
            crf::Log::error("Reason: {}", err);
            return;
        }

        crf::Log::info("glTF scene loaded successfully from: {}", filepath);
        if (!warn.empty()) {
            crf::Log::warn("Warnings: {}", warn);
        }
        crf::Log::info("Scene: {} meshes, {} nodes, {} scenes (default: {})",
            model.meshes.size(), model.nodes.size(), model.scenes.size(),
            model.defaultScene);
        
        for (size_t i = 0; i < model.nodes.size(); i++) {
            const tinygltf::Node& node = model.nodes[i];
            crf::Log::info("Node {}: name='{}' mesh={}", i, node.name, node.mesh);
        }

        for (size_t m = 0; m < model.meshes.size(); m++) {
            const tinygltf::Mesh& mesh = model.meshes[m];

            for (size_t p = 0; p < mesh.primitives.size(); p++) {
                const tinygltf::Primitive& primitive = mesh.primitives[p];
                crf::Log::info("mesh {} primitive {}: {} attributes, indices accessor = {}, material = {}",
                     m, p, primitive.attributes.size(), primitive.indices, primitive.material);

                for(const auto& [name, accessorIndex] : primitive.attributes) {
                    crf::Log::info("  attribute: {} -> accessor {}", name, accessorIndex);
                }

                int posAccessor = primitive.attributes.at("POSITION");
                const tinygltf::Accessor& accessor = model.accessors[posAccessor];
                crf::Log::info("POSITION: componentType={}, type={}, count={}, bufferView={}",
                    accessor.componentType, accessor.type, accessor.count, accessor.bufferView);
                
                const tinygltf::BufferView& bv = model.bufferViews[accessor.bufferView];
                const std::vector<unsigned char>& data = model.buffers[bv.buffer].data;
                size_t byteStride = accessor.ByteStride(bv);

                std::vector<float> positions(accessor.count * 3);
                for (size_t v = 0; v < accessor.count; v++) {
                    const unsigned char* src = data.data() + bv.byteOffset + accessor.byteOffset + v * byteStride;
                    positions[v * 3 + 0] = *reinterpret_cast<const float*>(src);
                    positions[v * 3 + 1] = *reinterpret_cast<const float*>(src + sizeof(float));
                    positions[v * 3 + 2] = *reinterpret_cast<const float*>(src + 2 * sizeof(float));
                }
                crf::Log::info("POSITION: decoded {} vertices, first = ({}, {}, {})",
                    accessor.count, positions[0], positions[1], positions[2]);
            }
        }
    }
}