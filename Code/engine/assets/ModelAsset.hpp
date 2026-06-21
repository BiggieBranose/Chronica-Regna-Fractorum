#pragma once
#include "AssetManager.hpp"
#include <string>
#include <vector>
#include <glm/glm.hpp>

namespace crf {

struct MeshData {
    std::vector<glm::vec3> positions;
    std::vector<glm::vec2> uvs;
    std::vector<glm::vec3> normals;
    std::vector<uint32_t> indices;
};

class ModelAsset : public Asset {
public:
    bool load(std::string_view path) override;
    void unload() override;

    const std::vector<MeshData>& getMeshes() const { return m_meshes; }

private:
    std::vector<MeshData> m_meshes;
};

} // namespace crf
