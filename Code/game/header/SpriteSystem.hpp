#pragma once

#include <graphics/VulkanContext.hpp>
#include <graphics/VulkanRenderPass.hpp>
#include <graphics/VulkanBuffer.hpp>
#include <vector>

namespace game {

struct SpriteVertex {
    float pos[2];
    float uv[2];
};

class SpriteSystem {
public:
    SpriteSystem(crf::VulkanContext& context, crf::VulkanRenderPass& renderPass);
    ~SpriteSystem();

    SpriteSystem(const SpriteSystem&) = delete;
    SpriteSystem& operator=(const SpriteSystem&) = delete;

    void init();
    void render(VkCommandBuffer cmd, crf::u32 imageIndex,
                const float* modelPtr, const float* viewPtr, const float* projPtr);

    VkPipeline getPipeline() const { return m_pipeline; }
    VkPipelineLayout getPipelineLayout() const { return m_pipelineLayout; }

private:
    void createQuadBuffer();
    void createTexture();
    void createUniformBuffers();
    void createDescriptorSetLayout();
    void createDescriptorPool();
    void createDescriptorSets();
    void createPipelineLayout();
    void createPipeline();

    crf::VulkanContext& m_context;
    crf::VulkanRenderPass& m_renderPass;

    VkPipelineLayout m_pipelineLayout = nullptr;
    VkPipeline m_pipeline = nullptr;

    VkDescriptorSetLayout m_descSetLayout = nullptr;
    VkDescriptorPool m_descPool = nullptr;
    std::vector<VkDescriptorSet> m_descSets;

    VkBuffer m_quadVB = nullptr;
    VkDeviceMemory m_quadVBMemory = nullptr;

    VkImage m_textureImage = nullptr;
    VkDeviceMemory m_textureMemory = nullptr;
    VkImageView m_textureView = nullptr;
    VkSampler m_sampler = nullptr;

    std::vector<VkBuffer> m_uniformBuffers;
    std::vector<VkDeviceMemory> m_uniformBuffersMemory;
    std::vector<void*> m_uniformBuffersMapped;
};

}
