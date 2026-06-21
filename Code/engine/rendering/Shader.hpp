#pragma once
#include <vulkan/vulkan_raii.hpp>
#include <string>
#include <vector>
#include <span>

namespace crf {

struct ShaderStage {
    vk::ShaderStageFlagBits stage;
    std::string entryPoint;
    std::vector<uint32_t> spirv;
};

class Shader {
public:
    Shader() = default;

    bool loadSPIRV(vk::raii::Device& device, const std::string& filepath, vk::ShaderStageFlagBits stage, std::string_view entry = "main");
    bool loadSPIRV(vk::raii::Device& device, std::span<const uint32_t> code, vk::ShaderStageFlagBits stage, std::string_view entry = "main");

    vk::ShaderModule getModule() const { return *m_module; }
    vk::ShaderStageFlagBits getStage() const { return m_stage; }
    const std::string& getEntryPoint() const { return m_entryPoint; }

    void destroy(vk::Device device);

private:
    vk::raii::ShaderModule m_module = nullptr;
    vk::ShaderStageFlagBits m_stage{};
    std::string m_entryPoint;
};

} // namespace crf
