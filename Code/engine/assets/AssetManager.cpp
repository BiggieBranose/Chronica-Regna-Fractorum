#include "AssetManager.hpp"
#include "../core/Log.hpp"

namespace crf {

AssetManager::~AssetManager() {
    unloadAll();
}

void AssetManager::unloadAll() {
    for (auto& [path, asset] : m_assets)
        asset->unload();
    m_assets.clear();
    Log::info("All assets unloaded");
}

} // namespace crf
