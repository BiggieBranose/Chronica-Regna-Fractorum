#include "BindlessManager.hpp"
#include "core/Log.hpp"
#include "core/Assert.hpp"
#include <vulkan/vulkan.h>
#include <algorithm>

namespace crf {

BindlessDescriptorManager::BindlessDescriptorManager(VulkanContext& context) : m_context(context) {
    m_freeTextureSlots.reserve(MAX_TEXTURES);
    m_freeBufferSlots.reserve(MAX_BUFFERS);
    for (uint32_t i = 0; i < MAX_TEXTURES; ++i) m_freeTextureSlots.push_back(i);
    for (uint32_t i = 0; i < MAX_BUFFERS; ++i) m_freeBufferSlots.push_back(i);
}

BindlessDescriptorManager::~BindlessDescriptorManager() {
    cleanup();
}

VkDescriptorSetLayout BindlessDescriptorManager::createLayout() {
    VkDescriptorSetLayoutBinding bindings[2] = {
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_TEXTURES, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, MAX_BUFFERS, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
    };

    VkDescriptorBindingFlags flags[2] = {
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT,
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT,
    };

    VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlags{};
    bindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    bindingFlags.bindingCount = 2;
    bindingFlags.pBindingFlags = flags;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    layoutInfo.bindingCount = 2;
    layoutInfo.pBindings = bindings;
    layoutInfo.pNext = &bindingFlags;

    VkDescriptorSetLayout layout;
    VkResult result = vkCreateDescriptorSetLayout(m_context.getDevice(), &layoutInfo, nullptr, &layout);
    CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to create bindless descriptor set layout");
    return layout;
}

void BindlessDescriptorManager::init() {
    m_descriptorSetLayout = createLayout();

    VkDescriptorPoolSize poolSizes[2] = {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_TEXTURES},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, MAX_BUFFERS},
    };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes = poolSizes;

    VkResult result = vkCreateDescriptorPool(m_context.getDevice(), &poolInfo, nullptr, &m_descriptorPool);
    CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to create bindless descriptor pool");

    VkDescriptorSetLayout layouts[] = {m_descriptorSetLayout};
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = layouts;

    VkDescriptorBindingFlags flags[2] = {
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT,
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT,
    };

    VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlags{};
    bindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    bindingFlags.bindingCount = 2;
    bindingFlags.pBindingFlags = flags;

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = layouts;
    allocInfo.pNext = &bindingFlags;

    VkResult result = vkAllocateDescriptorSets(m_context.getDevice(), &allocInfo, &m_descriptorSet);
    CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to allocate bindless descriptor set");

    updateDescriptorSet();
    Log::info("Bindless manager initialized: {} textures, {} buffers", MAX_TEXTURES, MAX_BUFFERS);
}

void BindlessDescriptorManager::cleanup() {
    if (m_descriptorPool) {
        vkDestroyDescriptorPool(m_context.getDevice(), m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
    }
    if (m_descriptorSetLayout) {
        vkDestroyDescriptorSetLayout(m_context.getDevice(), m_descriptorSetLayout, nullptr);
        m_descriptorSetLayout = VK_NULL_HANDLE;
    }
    m_descriptorSet = VK_NULL_HANDLE;
    m_textureCount = 0;
    m_bufferCount = 0;
    m_freeTextureSlots.clear();
    m_freeBufferSlots.clear();
    m_freeTextureSlots.reserve(MAX_TEXTURES);
    m_freeBufferSlots.reserve(MAX_BUFFERS);
    for (uint32_t i = 0; i < MAX_TEXTURES; ++i) m_freeTextureSlots.push_back(i);
    for (uint32_t i = 0; i < MAX_BUFFERS; ++i) m_freeBufferSlots.push_back(i);
}

void BindlessDescriptorManager::updateDescriptorSet() {
    std::vector<VkDescriptorImageInfo> imageInfos(MAX_TEXTURES);
    for (uint32_t i = 0; i < MAX_TEXTURES; ++i) {
        imageInfos[i].sampler = m_textures[i].sampler;
        imageInfos[i].imageView = m_textures[i].view;
        imageInfos[i].imageLayout = m_textures[i].layout;
    }

    std::vector<VkDescriptorBufferInfo> bufferInfos(MAX_BUFFERS);
    for (uint32_t i = 0; i < MAX_BUFFERS; ++i) {
        bufferInfos[i].buffer = m_buffers[i].buffer;
        bufferInfos[i].offset = m_buffers[i].offset;
        bufferInfos[i].range = m_buffers[i].range;
    }

    VkWriteDescriptorSet writes[2] = {};

    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = m_descriptorSet;
    writes[0].dstBinding = 0;
    writes[0].dstArrayElement = 0;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[0].descriptorCount = MAX_TEXTURES;
    writes[0].pImageInfo = imageInfos.data();

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = m_descriptorSet;
    writes[1].dstBinding = 1;
    writes[1].dstArrayElement = 0;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[1].descriptorCount = MAX_BUFFERS;
    writes[1].pBufferInfo = bufferInfos.data();

    vkUpdateDescriptorSets(m_context.getDevice(), 2, writes, 0, nullptr);
}

uint32_t BindlessDescriptorManager::addTexture(VkImageView view, VkSampler sampler, VkImageLayout layout) {
    CRF_ASSERT_MSG(!m_freeTextureSlots.empty(), "Texture pool exhausted");
    uint32_t index = m_freeTextureSlots.back();
    m_freeTextureSlots.pop_back();

    m_textures[index] = {view, sampler, layout};
    m_textureCount = std::max(m_textureCount, index + 1);
    return index;
}

void BindlessDescriptorManager::removeTexture(uint32_t index) {
    if (index >= MAX_TEXTURES) return;
    m_textures[index] = {};
    m_freeTextureSlots.push_back(index);
}

void BindlessDescriptorManager::updateTexture(uint32_t index, VkImageView view, VkSampler sampler, VkImageLayout layout) {
    if (index >= MAX_TEXTURES) return;
    m_textures[index] = {view, sampler, layout};
}

uint32_t BindlessDescriptorManager::addBuffer(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range) {
    CRF_ASSERT_MSG(!m_freeBufferSlots.empty(), "Buffer pool exhausted");
    uint32_t index = m_freeBufferSlots.back();
    m_freeBufferSlots.pop_back();

    m_buffers[index] = {buffer, offset, range};
    m_bufferCount = std::max(m_bufferCount, index + 1);
    return index;
}

void BindlessDescriptorManager::removeBuffer(uint32_t index) {
    if (index >= MAX_BUFFERS) return;
    m_buffers[index] = {};
    m_freeBufferSlots.push_back(index);
}

void BindlessDescriptorManager::updateBuffer(uint32_t index, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range) {
    if (index >= MAX_BUFFERS) return;
    m_buffers[index] = {buffer, offset, range};
}

void BindlessDescriptorManager::cleanup() {
    if (m_descriptorPool) {
        vkDestroyDescriptorPool(m_context.getDevice(), m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
    }
    if (m_descriptorSetLayout) {
        vkDestroyDescriptorSetLayout(m_context.getDevice(), m_descriptorSetLayout, nullptr);
        m_descriptorSetLayout = VK_NULL_HANDLE;
    }
    m_descriptorSet = VK_NULL_HANDLE;
}

}