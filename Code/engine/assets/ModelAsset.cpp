#include "ModelAsset.hpp"
#include "../core/Log.hpp"

namespace crf {

bool ModelAsset::load(std::string_view path) {
    m_path = path;
    Log::info("Model loading not yet implemented: {}", path);
    return false;
}

void ModelAsset::unload() {
    m_meshes.clear();
    m_valid = false;
}

} // namespace crf
