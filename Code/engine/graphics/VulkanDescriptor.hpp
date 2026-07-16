#pragma once

#include "core/Types.hpp"
#include "VulkanContext.hpp"
#include "VulkanBuffer.hpp"
#include <vector>

namespace crf {

class VulkanDescriptor {
public:
    VulkanDescriptor(VulkanContext& context, VkDescriptorSetLayout descriptorSetLayout, VkDescriptorPool descriptorPool);
    ~VulkanDescriptor();

    VulkanDescriptor(const VulkanDescriptor&) = delete;
    VulkanDescriptor& operator=(const VulkanDescriptor&) = delete;
    VulkanDescriptor(VulkanDescriptor&&) = delete;
    VulkanDescriptor& operator=(VulkanDescriptor&&) = delete;

    void createDescriptorPool(u32 poolSize);
    void createDescriptorSets(const std::vector<VkBuffer>& uniformBuffers, u32 bufferCount);

    VkDescriptorPool getDescriptorPool() const { return m_descriptorPool; }
    const std::vector<VkDescriptorSet>& getDescriptorSets() const { return m_descriptorSets; }

private:
    VulkanContext& m_context;
    VkDescriptorSetLayout m_descriptorSetLayout;
    VkDescriptorPool m_descriptorPool = nullptr;
    std::vector<VkDescriptorSet> m_descriptorSets;
};

}
