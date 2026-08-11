#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_IMPLEMENTATION
#include "tinygltf/tiny_gltf.h"

#include "GlTFLoader.hpp"
#include <core/Log.hpp>

#include <map>
#include <array>
#include <functional>
#include <limits>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

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

        for (size_t i = 0; i < model.images.size(); i++) {
            const tinygltf::Image& image = model.images[i];
            ImageData imageData;
            imageData.width = static_cast<u32>(image.width);
            imageData.height = static_cast<u32>(image.height);
            imageData.pixels = image.image;
            meshData.images.push_back(imageData);
            crf::Log::info("Image {}: name='{}' {}x{} ({} bytes of {} channels)",
                i, image.name, image.width, image.height, image.image.size(), image.component);
        }

        std::vector<u32> meshPrimStart(model.meshes.size(), 0);
        std::vector<u32> meshPrimCount(model.meshes.size(), 0);
        std::vector<u32> meshVertStart(model.meshes.size(), 0);
        std::vector<u32> meshVertCount(model.meshes.size(), 0);

        for (size_t m = 0; m < model.meshes.size(); m++) {
            const tinygltf::Mesh& mesh = model.meshes[m];
            meshPrimStart[m] = static_cast<u32>(meshData.primitives.size());
            meshVertStart[m] = static_cast<u32>(meshData.positions.size() / 3);

            for (size_t p = 0; p < mesh.primitives.size(); p++) {
                const tinygltf::Primitive& primitive = mesh.primitives[p];
                crf::Log::info("mesh {} primitive {}: {} attributes, indices accessor = {}, material = {}",
                     m, p, primitive.attributes.size(), primitive.indices, primitive.material);

                for(const auto& [name, accessorIndex] : primitive.attributes) {
                    crf::Log::info("  attribute: {} -> accessor {}", name, accessorIndex);
                }

                PrimitiveData primData;
                primData.firstIndex = static_cast<u32>(meshData.indices.size());

                u32 textureIndex = 0;
                if (primitive.material >= 0 && static_cast<size_t>(primitive.material) < model.materials.size()) {
                    const tinygltf::Material& material = model.materials[primitive.material];
                    const tinygltf::TextureInfo& baseColor = material.pbrMetallicRoughness.baseColorTexture;
                    if (baseColor.index >= 0 && static_cast<size_t>(baseColor.index) < model.textures.size()) {
                        int sourceImage = model.textures[baseColor.index].source;
                        if (sourceImage >= 0 && static_cast<size_t>(sourceImage) < meshData.images.size()) {
                            textureIndex = static_cast<u32>(sourceImage);
                        }
                    }
                }
                primData.textureIndex = textureIndex;
                crf::Log::info("  texture index = {}", textureIndex);

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

                if (primitive.attributes.count("NORMAL") > 0) {
                    int normalAccessorIndex = primitive.attributes.at("NORMAL");
                    const tinygltf::Accessor& normalAccessor = model.accessors[normalAccessorIndex];
                    const tinygltf::BufferView& normalBV = model.bufferViews[normalAccessor.bufferView];
                    const std::vector<unsigned char>& normalData = model.buffers[normalBV.buffer].data;
                    size_t normalByteStride = normalAccessor.ByteStride(normalBV);

                    std::vector<float> normals(normalAccessor.count * 3);
                    for (size_t v = 0; v < normalAccessor.count; v++) {
                        const unsigned char* src = normalData.data() + normalBV.byteOffset + normalAccessor.byteOffset + v * normalByteStride;
                        normals[v * 3 + 0] = *reinterpret_cast<const float*>(src);
                        normals[v * 3 + 1] = *reinterpret_cast<const float*>(src + sizeof(float));
                        normals[v * 3 + 2] = *reinterpret_cast<const float*>(src + 2 * sizeof(float));
                    }
                    meshData.normals.insert(meshData.normals.end(), normals.begin(), normals.end());
                } else {
                    meshData.normals.insert(meshData.normals.end(), posAccessor.count * 3, 0.0f);
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
                primData.indexCount = static_cast<u32>(idxAccessor.count);
                meshData.primitives.push_back(primData);
                crf::Log::info("INDICES: decoded {} indices", meshData.indices.size());
            }

            meshPrimCount[m] = static_cast<u32>(meshData.primitives.size()) - meshPrimStart[m];
            meshVertCount[m] = static_cast<u32>(meshData.positions.size() / 3) - meshVertStart[m];
        }

        // Compute world transforms for all nodes, then expose node info for scene building.
        const std::vector<tinygltf::Node>& gltfNodes = model.nodes;
        std::vector<glm::mat4> localMatrices(gltfNodes.size(), glm::mat4(1.0f));
        std::vector<glm::mat4> worldMatrices(gltfNodes.size(), glm::mat4(1.0f));
        std::vector<bool> hasParent(gltfNodes.size(), false);

        for (size_t i = 0; i < gltfNodes.size(); i++) {
            const tinygltf::Node& node = gltfNodes[i];

            glm::mat4 local(1.0f);
            if (node.matrix.size() == 16) {
                for (int c = 0; c < 4; c++) {
                    for (int r = 0; r < 4; r++) {
                        local[c][r] = static_cast<f32>(node.matrix[c * 4 + r]);
                    }
                }
            } else {
                if (node.translation.size() == 3) {
                    local = glm::translate(local, glm::vec3(
                        static_cast<f32>(node.translation[0]),
                        static_cast<f32>(node.translation[1]),
                        static_cast<f32>(node.translation[2])));
                }
                if (node.rotation.size() == 4) {
                    glm::quat q(
                        static_cast<f32>(node.rotation[3]),
                        static_cast<f32>(node.rotation[0]),
                        static_cast<f32>(node.rotation[1]),
                        static_cast<f32>(node.rotation[2]));
                    local *= glm::mat4_cast(q);
                }
                if (node.scale.size() == 3) {
                    local = glm::scale(local, glm::vec3(
                        static_cast<f32>(node.scale[0]),
                        static_cast<f32>(node.scale[1]),
                        static_cast<f32>(node.scale[2])));
                }
            }
            localMatrices[i] = local;

            for (int child : node.children) {
                if (child >= 0 && static_cast<size_t>(child) < gltfNodes.size()) {
                    hasParent[child] = true;
                }
            }
        }

        std::function<void(size_t, const glm::mat4&)> propagate =
            [&](size_t index, const glm::mat4& parentWorld) {
                worldMatrices[index] = parentWorld * localMatrices[index];
                for (int child : gltfNodes[index].children) {
                    if (child >= 0 && static_cast<size_t>(child) < gltfNodes.size()) {
                        propagate(static_cast<size_t>(child), worldMatrices[index]);
                    }
                }
            };

        for (size_t i = 0; i < gltfNodes.size(); i++) {
            if (!hasParent[i]) {
                propagate(i, glm::mat4(1.0f));
            }
        }

        for (size_t i = 0; i < gltfNodes.size(); i++) {
            const tinygltf::Node& node = gltfNodes[i];
            if (node.mesh < 0 || static_cast<size_t>(node.mesh) >= model.meshes.size()) {
                continue;
            }

            u32 meshIndex = static_cast<u32>(node.mesh);
            NodeData nodeData;
            nodeData.name = node.name;
            nodeData.worldTransform = worldMatrices[i];
            nodeData.meshIndex = meshIndex;
            nodeData.firstPrimitive = meshPrimStart[meshIndex];
            nodeData.primitiveCount = meshPrimCount[meshIndex];

            glm::vec3 minLocal(std::numeric_limits<f32>::max());
            glm::vec3 maxLocal(std::numeric_limits<f32>::lowest());
            for (u32 v = 0; v < meshVertCount[meshIndex]; v++) {
                const u32 base = (meshVertStart[meshIndex] + v) * 3;
                glm::vec3 p(meshData.positions[base + 0],
                            meshData.positions[base + 1],
                            meshData.positions[base + 2]);
                minLocal = glm::min(minLocal, p);
                maxLocal = glm::max(maxLocal, p);
            }

            glm::vec3 minWorld(std::numeric_limits<f32>::max());
            glm::vec3 maxWorld(std::numeric_limits<f32>::lowest());
            for (u32 corner = 0; corner < 8; corner++) {
                glm::vec3 local((corner & 1) ? maxLocal.x : minLocal.x,
                                (corner & 2) ? maxLocal.y : minLocal.y,
                                (corner & 4) ? maxLocal.z : minLocal.z);
                glm::vec4 world = nodeData.worldTransform * glm::vec4(local, 1.0f);
                minWorld = glm::min(minWorld, glm::vec3(world));
                maxWorld = glm::max(maxWorld, glm::vec3(world));
            }
            nodeData.aabbMin = minWorld;
            nodeData.aabbMax = maxWorld;

            meshData.nodes.push_back(nodeData);
            crf::Log::info("Node '{}': mesh={} prims=[{},{}) aabb=({:.1f},{:.1f},{:.1f})-({:.1f},{:.1f},{:.1f})",
                nodeData.name, nodeData.meshIndex, nodeData.firstPrimitive,
                nodeData.firstPrimitive + nodeData.primitiveCount,
                nodeData.aabbMin.x, nodeData.aabbMin.y, nodeData.aabbMin.z,
                nodeData.aabbMax.x, nodeData.aabbMax.y, nodeData.aabbMax.z);
        }

        s_cache[filepath] = meshData;
        return s_cache.at(filepath);
    }
}