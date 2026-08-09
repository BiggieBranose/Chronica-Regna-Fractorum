#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_IMPLEMENTATION
#include "tinygltf/tiny_gltf.h"

#include "GlTFLoader.hpp"
#include <core/Log.hpp>

#include <map>

namespace crf {
    const MeshData& loadScene(const std::string& filepath) {
        static std::map<std::string, MeshData> s_cache;

        if (s_cache.count(filepath) != 0) {
            crf::Log::info("Loading {} from cache", filepath);
            return s_cache.at(filepath);
        }

        crf::Log::info("Loading glTF scene from: {}", filepath);
        tinygltf::Model model;
        tinygltf::TinyGLTF loader;
        std::string err;
        std::string warn;

        if (!loader.LoadBinaryFromFile(&model, &err, &warn, filepath)) {
            crf::Log::error("Failed to load glTF scene from: {}", filepath);
            crf::Log::error("Reason: {}", err);
            static MeshData s_empty;
            return s_empty;
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

        MeshData meshData;

        for (size_t m = 0; m < model.meshes.size(); m++) {
            const tinygltf::Mesh& mesh = model.meshes[m];

            for (size_t p = 0; p < mesh.primitives.size(); p++) {
                const tinygltf::Primitive& primitive = mesh.primitives[p];
                crf::Log::info("mesh {} primitive {}: {} attributes, indices accessor = {}, material = {}",
                     m, p, primitive.attributes.size(), primitive.indices, primitive.material);

                for(const auto& [name, accessorIndex] : primitive.attributes) {
                    crf::Log::info("  attribute: {} -> accessor {}", name, accessorIndex);
                }

                int posAccessorIndex = primitive.attributes.at("POSITION");
                const tinygltf::Accessor& posAccessor = model.accessors[posAccessorIndex];
                crf::Log::info("POSITION: componentType={}, type={}, count={}, bufferView={}",
                    posAccessor.componentType, posAccessor.type, posAccessor.count, posAccessor.bufferView);
                
                const tinygltf::BufferView& bv = model.bufferViews[posAccessor.bufferView];
                const std::vector<unsigned char>& data = model.buffers[bv.buffer].data;
                size_t baseVertex = meshData.positions.size() / 3;
                size_t byteStride = posAccessor.ByteStride(bv);

                std::vector<float> positions(posAccessor.count * 3);
                for (size_t v = 0; v < posAccessor.count; v++) {
                    const unsigned char* src = data.data() + bv.byteOffset + posAccessor.byteOffset + v * byteStride;
                    positions[v * 3 + 0] = *reinterpret_cast<const float*>(src);
                    positions[v * 3 + 1] = *reinterpret_cast<const float*>(src + sizeof(float));
                    positions[v * 3 + 2] = *reinterpret_cast<const float*>(src + 2 * sizeof(float));
                }
                crf::Log::info("POSITION: decoded {} vertices, first = ({}, {}, {})",
                    posAccessor.count, positions[0], positions[1], positions[2]);

                meshData.positions.insert(meshData.positions.end(), positions.begin(), positions.end());

                if (primitive.attributes.count("TEXCOORD_0") > 0) {
                    int texAccessorIndex = primitive.attributes.at("TEXCOORD_0");
                    const tinygltf::Accessor& texAccessor = model.accessors[texAccessorIndex];
                    const tinygltf::BufferView& texBV = model.bufferViews[texAccessor.bufferView];
                    const std::vector<unsigned char>& texData = model.buffers[texBV.buffer].data;
                    size_t texByteStride = texAccessor.ByteStride(texBV);

                    std::vector<float> texCoords(texAccessor.count * 2);
                    for (size_t v = 0; v < texAccessor.count; v++) {
                        const unsigned char* src = texData.data() + texBV.byteOffset + texAccessor.byteOffset + v * texByteStride;
                        texCoords[v * 2 + 0] = *reinterpret_cast<const float*>(src);
                        texCoords[v * 2 + 1] = *reinterpret_cast<const float*>(src + sizeof(float));
                    }
                    meshData.texCoords.insert(meshData.texCoords.end(), texCoords.begin(), texCoords.end());
                } else {
                    meshData.texCoords.insert(meshData.texCoords.end(), posAccessor.count * 2, 0.0f);
                }

                int indexAccessor = primitive.indices;
                const tinygltf::Accessor& idxAccessor = model.accessors[indexAccessor];
                crf::Log::info("INDICES: componentType={}, type={}, count={}, bufferView={}",
                    idxAccessor.componentType, idxAccessor.type, idxAccessor.count, idxAccessor.bufferView);
                
                const tinygltf::BufferView& idxBV = model.bufferViews[idxAccessor.bufferView];
                const std::vector<unsigned char>& idxData = model.buffers[idxBV.buffer].data;

                size_t idxStride = idxAccessor.ByteStride(idxBV);
                if (idxStride == 0) {
                    idxStride = (idxAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
                        ? sizeof(unsigned int) : sizeof(unsigned short);
                }

                for (size_t i = 0; i < idxAccessor.count; i++) {
                    const unsigned char* isrc = idxData.data() + idxBV.byteOffset + idxAccessor.byteOffset + i * idxStride;

                    unsigned int index = 0;
                    if (idxAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                        index = *reinterpret_cast<const unsigned int*>(isrc);
                    } else {
                        index = *reinterpret_cast<const unsigned short*>(isrc);
                    }
                    meshData.indices.push_back(baseVertex + index);
                }
                crf::Log::info("INDICES: decoded {} indices", meshData.indices.size());
            }
        }

        s_cache[filepath] = meshData;
        return s_cache.at(filepath);
    }
}