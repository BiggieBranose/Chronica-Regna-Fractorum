#include "AudioAsset.hpp"
#include "../core/Log.hpp"

namespace crf {

bool AudioAsset::load(std::string_view path) {
    m_path = path;
    Log::info("Audio loading not yet implemented: {}", path);
    return false;
}

void AudioAsset::unload() {
    m_samples.clear();
    m_valid = false;
}

} // namespace crf
