#include "TextureAsset.hpp"
#include "../rendering/Renderer.hpp"
#include "../rendering/Texture.hpp"
#include "../core/Log.hpp"

namespace crf {

bool TextureAsset::load(std::string_view path) {
    m_path = path;
    return false; // Texture loading requires Renderer access - load externally
}

void TextureAsset::unload() {
    m_texture.reset();
    m_valid = false;
}

} // namespace crf
