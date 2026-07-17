#pragma once

#include <graphics/VulkanContext.hpp>
#include <graphics/VulkanRenderPass.hpp>
#include <graphics/VulkanBuffer.hpp>
#include <vector>

namespace crf {

class Window;

}

namespace game {

class Game {
public:
    Game(crf::VulkanContext& context, crf::VulkanRenderPass& renderPass, crf::Window& window);
    ~Game();

    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;

    void init();

    void update(float dt);
    void render(VkCommandBuffer cmd, crf::u32 imageIndex, const float* viewPtr, const float* projPtr);

    float getCharX() const { return m_charX; }
    float getCharY() const { return m_charY; }
    float getCharZ() const { return m_charZ; }

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
    crf::Window& m_window;

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

    float m_time = 0.0f;
    float m_charX = 0.0f;
    float m_charY = -0.0f;
    float m_charZ = 0.0f;
    float m_charAngle = 0.0f;
};

}
