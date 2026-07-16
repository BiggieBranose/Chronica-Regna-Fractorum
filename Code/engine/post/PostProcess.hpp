#pragma once

#include "core/Types.hpp"
#include "graphics/VulkanContext.hpp"
#include "render/RenderGraph.hpp"
#include <vulkan/vulkan.h>
#include <memory>

namespace crf {

class PostProcess {
public:
    PostProcess(VulkanContext& context);
    ~PostProcess();

    PostProcess(const PostProcess&) = delete;
    PostProcess& operator=(const PostProcess&) = delete;

    void init(RenderGraph& graph, const std::string& colorInput, const std::string& finalOutput);
    void cleanup();

    void setExposure(float exposure) { m_exposure = exposure; }
    void setGamma(float gamma) { m_gamma = gamma; }
    void setBloomStrength(float strength) { m_bloomStrength = strength; }
    void setBloomThreshold(float threshold) { m_bloomThreshold = threshold; }
    void setVignetteStrength(float strength) { m_vignetteStrength = strength; }
    void setFilmGrain(float strength) { m_filmGrain = strength; }

private:
    VulkanContext& m_context;
    RenderGraph* m_graph = nullptr;
    std::string m_colorInput;
    std::string m_finalOutput;

    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    std::array<VkPipeline, 4> m_pipelines{};
    VkDescriptorSetLayout m_descSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_descPool = VK_NULL_HANDLE;
    VkDescriptorSet m_descSet = VK_NULL_HANDLE;
    VkSampler m_sampler = VK_NULL_HANDLE;

    VkImage m_bloomImage = VK_NULL_HANDLE;
    VmaAllocation m_bloomAlloc = VK_NULL_HANDLE;
    VkImageView m_bloomView = VK_NULL_HANDLE;

    struct PushConstants {
        float exposure = 1.0f;
        float gamma = 2.2f;
        float bloomStrength = 0.15f;
        float bloomThreshold = 1.0f;
        float vignetteStrength = 0.3f;
        float filmGrain = 0.02f;
        float time = 0.0f;
        float padding[2];
    } m_pushConstants{};

    void createResources();
    void createDescriptorSet();
    void createPipelines();
    void createSampler();
    void destroyResources();

    VkPipeline createPipeline(const std::string& vertPath, const std::string& fragPath);
};

}