#pragma once
#include "AssetManager.hpp"
#include "../rendering/Texture.hpp"

namespace crf {

class Renderer;

class TextureAsset : public Asset {
public:
    bool load(std::string_view path) override;
    void unload() override;

    Texture* getTexture() { return m_texture.get(); }

private:
    std::unique_ptr<Texture> m_texture;
};

} // namespace crf
