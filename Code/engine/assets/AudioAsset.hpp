#pragma once
#include "AssetManager.hpp"
#include <vector>
#include <cstdint>

namespace crf {

class AudioAsset : public Asset {
public:
    bool load(std::string_view path) override;
    void unload() override;

    const std::vector<int16_t>& getSamples() const { return m_samples; }
    int getSampleRate() const { return m_sampleRate; }
    int getChannels() const { return m_channels; }

private:
    std::vector<int16_t> m_samples;
    int m_sampleRate = 44100;
    int m_channels = 2;
};

} // namespace crf
