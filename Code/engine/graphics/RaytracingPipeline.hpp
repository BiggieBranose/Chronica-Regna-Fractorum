#pragma once

#include "core/Types.hpp"
#include "VulkanContext.hpp"
#include "AccelerationStructure.hpp"
#include "VulkanBuffer.hpp"
#include "VulkanPipeline.hpp"
#include <vector>

struct VkPipeline_T;
using VkPipeline = VkPipeline_T*;
struct VkShaderModule_T;
using VkShaderModule = VkShaderModule_T*;

namespace crf {

struct RaytracingPushConstants {
    f32 clearColor[4];
    u32 maxRecursionDepth;
    f32 padding[2];
};

class RaytracingPipeline {
public:
    RaytracingPipeline(VulkanContext& context, AccelerationStructure& accelStruct);
    ~RaytracingPipeline();

    RaytracingPipeline(const RaytracingPipeline&) = delete;
    RaytracingPipeline& operator=(const RaytracingPipeline&) = delete;
    RaytracingPipeline(RaytracingPipeline&&) = delete;
    RaytracingPipeline& operator=(RaytracingPipeline&&) = delete;

    void createRaytracingPipeline();
    void createShaderBindingTable();
    void createRaytracingDescriptorSetLayout();
    void createRaytracingDescriptorPool();
    void createRaytracingDescriptorSets(VkImageView outputImageView, VkSampler outputSampler,
                                         VkBuffer vertexBuffer, VkDeviceSize vertexBufferSize,
                                         VkBuffer cameraBuffer, VkDeviceSize cameraBufferSize);

    void recordRaytracingCommands(VkCommandBuffer commandBuffer, u32 width, u32 height);

    VkPipeline getPipeline() const { return m_pipeline; }
    VkPipelineLayout getPipelineLayout() const { return m_pipelineLayout; }
    VkDescriptorSetLayout getDescriptorSetLayout() const { return m_descriptorSetLayout; }

private:
    VkDeviceAddress getBufferDeviceAddress(VkBuffer buffer);

    struct ShaderBindingTableEntry {
        std::vector<u8> shaderGroupHandle;
    };

    VulkanContext& m_context;
    AccelerationStructure& m_accelStruct;
    VkPipeline m_pipeline = nullptr;
    VkPipelineLayout m_pipelineLayout = nullptr;
    VkDescriptorSetLayout m_descriptorSetLayout = nullptr;
    VkDescriptorPool m_descriptorPool = nullptr;
    std::vector<VkDescriptorSet> m_descriptorSets;

    VkBuffer m_shaderBindingTableBuffer = nullptr;
    VkDeviceMemory m_shaderBindingTableMemory = nullptr;
    u32 m_shaderGroupBaseAlignment = 0;
    u32 m_shaderGroupHandleSize = 0;
    u32 m_shaderGroupHandleAlignment = 0;
};

}
