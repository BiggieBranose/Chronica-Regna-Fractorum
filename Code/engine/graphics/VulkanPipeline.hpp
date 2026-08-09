#pragma once

#include "core/Types.hpp"
#include "VulkanContext.hpp"
#include "Vertex.hpp"
#include <vector>
#include <optional>

namespace crf {

class VulkanPipeline {
public:
    VulkanPipeline(VulkanContext& context, VkRenderPass renderPass, VkSampleCountFlagBits msaaSamples);
    ~VulkanPipeline();

    VulkanPipeline(const VulkanPipeline&) = delete;
    VulkanPipeline& operator=(const VulkanPipeline&) = delete;
    VulkanPipeline(VulkanPipeline&&) = delete;
    VulkanPipeline& operator=(VulkanPipeline&&) = delete;

    void createGraphicsPipeline(const std::string& vertShaderPath = "shaders/cube.vert.spv", const std::string& fragShaderPath = "shaders/cube.frag.spv");
    void createSkyPipeline();
    void createDescriptorSetLayout();
    void createRayQueryDescriptorSetLayout();
    void createPipelineLayout(VkDescriptorSetLayout descriptorSetLayout);

    VkPipeline getGraphicsPipeline() const { return m_graphicsPipeline; }
    VkPipeline getSkyPipeline() const { return m_skyPipeline; }
    VkPipelineLayout getPipelineLayout() const { return m_pipelineLayout; }
    VkDescriptorSetLayout getDescriptorSetLayout() const { return m_descriptorSetLayout; }

    static VkShaderModule createShaderModule(VkDevice device, const std::vector<u8>& code);
    static std::vector<u8> readFile(const std::string& filepath);

private:
    VulkanContext& m_context;
    VkRenderPass m_renderPass;
    VkSampleCountFlagBits m_msaaSamples;
    VkPipeline m_graphicsPipeline = nullptr;
    VkPipeline m_skyPipeline = nullptr;
    VkPipelineLayout m_pipelineLayout = nullptr;
    VkDescriptorSetLayout m_descriptorSetLayout = nullptr;
};

}
