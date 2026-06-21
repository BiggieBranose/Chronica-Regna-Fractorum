#include "Shader.hpp"
#include "../core/Log.hpp"
#include "../core/File.hpp"
#include <fstream>

namespace crf {

bool Shader::loadSPIRV(vk::raii::Device& device, const std::string& filepath, vk::ShaderStageFlagBits stage, std::string_view entry) {
    auto data = File::readBinary(filepath);
    if (!data) {
        Log::error("Failed to read shader file: {}", filepath);
        return false;
    }
    return loadSPIRV(device, std::span<const uint32_t>(
        reinterpret_cast<const uint32_t*>(data->data()),
        data->size() / sizeof(uint32_t)), stage, entry);
}

bool Shader::loadSPIRV(vk::raii::Device& device, std::span<const uint32_t> code, vk::ShaderStageFlagBits stage, std::string_view entry) {
    m_stage = stage;
    m_entryPoint = entry;

    vk::ShaderModuleCreateInfo info{};
    info.codeSize = code.size() * sizeof(uint32_t);
    info.pCode = code.data();
    info.pNext = nullptr;

    try {
        m_module = vk::raii::ShaderModule(device, info);
    } catch (const std::exception& e) {
        Log::error("Failed to create shader module: {}", e.what());
        return false;
    }
    return true;
}

void Shader::destroy(vk::Device device) {
    (void)device;
    m_module = nullptr;
}

} // namespace crf
