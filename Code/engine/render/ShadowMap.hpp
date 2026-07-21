#pragma once

#include "core/Types.hpp"
#include "graphics/VulkanContext.hpp"
#include "graphics/VulkanPipeline.hpp"
#include "render/BindlessManager.hpp"
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <array>

namespace crf {

class ShadowMap {
public:
    static constexpr uint32_t CASCADE_COUNT = 4;
    static constexpr VkFormat DEPTH_FORMAT = VK_FORMAT_D32_SFLOAT;

    struct Cascade {
        float splitDepth;
        glm::mat4 viewProj;
        VkImageView view;
    };

    struct PushConstants {
        glm::mat4 cascadeViewProj[CASCADE_COUNT];
        float cascadeSplits[CASCADE_COUNT];
    };

    ShadowMap(VulkanContext& context, uint32_t resolution = 2048);
    ~ShadowMap();

    ShadowMap(const ShadowMap&) = delete;
    ShadowMap& operator=(const ShadowMap&) = delete;

    void init();
    void cleanup();
    void updateCascades(const glm::vec3& lightDir, const glm::mat4& view, const glm::mat4& proj, float nearPlane, float farPlane);
    void render(VkCommandBuffer cmd, const std::function<void(VkCommandBuffer)>& drawOpaque);

    const std::array<Cascade, CASCADE_COUNT>& getCascades() const { return m_cascades; }
    VkImageView getDepthArrayView() const { return m_depthArrayView; }
    VkExtent2D getResolution() const { return m_resolution; }
    const PushConstants& getPushConstants() const { return m_pushConstants; }

private:
    VulkanContext& m_context;
    VkExtent2D m_resolution;
    std::array<Cascade, CASCADE_COUNT> m_cascades;
    PushConstants m_pushConstants{};

    VkImage m_depthArray = VK_NULL_HANDLE;
    VmaAllocation m_depthArrayAlloc = VK_NULL_HANDLE;
    VkImageView m_depthArrayView = VK_NULL_HANDLE;
    std::array<VkImageView, CASCADE_COUNT> m_cascadeViews{};

    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    VkFramebuffer m_framebuffer = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkPipeline m_pipelineNoCull = VK_NULL_HANDLE;

    VkDescriptorSetLayout m_descSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_descPool = VK_NULL_HANDLE;
    VkDescriptorSet m_descSet = VK_NULL_HANDLE;

    VkSampler m_shadowSampler = VK_NULL_HANDLE;

    float m_cascadeSplits[4] = {0.1f, 0.3f, 0.6f, 1.0f};

    void createResources();
    void createRenderPass();
    void createFramebuffer();
    void createPipeline();
    void createDescriptorSet();
    void createSampler();
    void destroyResources();
};

}