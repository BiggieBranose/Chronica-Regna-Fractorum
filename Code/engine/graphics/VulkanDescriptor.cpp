#include "VulkanDescriptor.hpp"
#include "core/Log.hpp"
#include "core/Assert.hpp"

#include <array>
#include <vulkan/vulkan.h>

namespace crf {

VulkanDescriptor::VulkanDescriptor(VulkanContext& context, VkDescriptorSetLayout descriptorSetLayout, VkDescriptorPool descriptorPool)
    : m_context(context), m_descriptorSetLayout(descriptorSetLayout), m_descriptorPool(descriptorPool) {
}

VulkanDescriptor::~VulkanDescriptor() {
    if (m_descriptorPool) {
        vkDestroyDescriptorPool(m_context.getDevice(), m_descriptorPool, nullptr);
    }
}

void VulkanDescriptor::createDescriptorPool(u32 poolSize) {
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = static_cast<u32>(poolSize);
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = static_cast<u32>(poolSize);

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<u32>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = static_cast<u32>(poolSize);

    VkResult result = vkCreateDescriptorPool(m_context.getDevice(), &poolInfo, nullptr, &m_descriptorPool);
    CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to create descriptor pool");
}

void VulkanDescriptor::createDescriptorSets(const std::vector<VkBuffer>& uniformBuffers, u32 bufferCount, VkImageView textureImageView, VkSampler textureSampler) {
    std::vector<VkDescriptorSetLayout> layouts(bufferCount, m_descriptorSetLayout);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = static_cast<u32>(bufferCount);
    allocInfo.pSetLayouts = layouts.data();

    m_descriptorSets.resize(bufferCount);

    VkResult result = vkAllocateDescriptorSets(m_context.getDevice(), &allocInfo, m_descriptorSets.data());
    CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to allocate descriptor sets");

    for (u32 i = 0; i < bufferCount; i++) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = uniformBuffers[i];
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = textureImageView;
        imageInfo.sampler = textureSampler;

        std::array<VkWriteDescriptorSet, 2> writes{};

        writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet = m_descriptorSets[i];
        writes[0].dstBinding = 0;
        writes[0].dstArrayElement = 0;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[0].descriptorCount = 1;
        writes[0].pBufferInfo = &bufferInfo;

        writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet = m_descriptorSets[i];
        writes[1].dstBinding = 1;
        writes[1].dstArrayElement = 0;
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[1].descriptorCount = 1;
        writes[1].pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(m_context.getDevice(), static_cast<u32>(writes.size()), writes.data(), 0, nullptr);
    }
}

}
