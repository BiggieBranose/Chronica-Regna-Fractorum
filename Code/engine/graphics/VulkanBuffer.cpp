#include "VulkanBuffer.hpp"
#include "core/Log.hpp"
#include "core/Assert.hpp"

#include <cstring>
#include <vulkan/vulkan.h>

namespace crf {

VulkanBuffer::VulkanBuffer(VulkanContext& context, VkCommandPool commandPool)
    : m_context(context), m_commandPool(commandPool) {
}

VulkanBuffer::~VulkanBuffer() {
    destroyBuffers();
}

void VulkanBuffer::destroyBuffers() {
    VkDevice device = m_context.getDevice();

    for (u32 i = 0; i < m_uniformBuffers.size(); i++) {
        vkDestroyBuffer(device, m_uniformBuffers[i], nullptr);
        vkFreeMemory(device, m_uniformBuffersMemory[i], nullptr);
    }
    m_uniformBuffers.clear();
    m_uniformBuffersMemory.clear();
    m_uniformBuffersMapped.clear();

    if (m_indexBuffer) {
        vkDestroyBuffer(device, m_indexBuffer, nullptr);
        vkFreeMemory(device, m_indexBufferMemory, nullptr);
        m_indexBuffer = nullptr;
    }

    if (m_vertexBuffer) {
        vkDestroyBuffer(device, m_vertexBuffer, nullptr);
        vkFreeMemory(device, m_vertexBufferMemory, nullptr);
        m_vertexBuffer = nullptr;
    }
}

void VulkanBuffer::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                                VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
    VkDevice device = m_context.getDevice();

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkResult result = vkCreateBuffer(device, &bufferInfo, nullptr, &buffer);
    CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to create buffer");

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties, m_context.getPhysicalDevice());

    result = vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory);
    CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to allocate buffer memory");

    vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

void VulkanBuffer::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommandsInternal();

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    endSingleTimeCommandsInternal(commandBuffer);
}

void VulkanBuffer::createVertexBuffer(const std::vector<Vertex>& vertices) {
    VkDeviceSize bufferSize = sizeof(Vertex) * vertices.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer, stagingMemory);

    void* data;
    vkMapMemory(m_context.getDevice(), stagingMemory, 0, bufferSize, 0, &data);
    std::memcpy(data, vertices.data(), bufferSize);
    vkUnmapMemory(m_context.getDevice(), stagingMemory);

    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 m_vertexBuffer, m_vertexBufferMemory);

    copyBuffer(stagingBuffer, m_vertexBuffer, bufferSize);

    vkDestroyBuffer(m_context.getDevice(), stagingBuffer, nullptr);
    vkFreeMemory(m_context.getDevice(), stagingMemory, nullptr);
}

void VulkanBuffer::createIndexBuffer(const std::vector<u32>& indices) {
    VkDeviceSize bufferSize = sizeof(u32) * indices.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer, stagingMemory);

    void* data;
    vkMapMemory(m_context.getDevice(), stagingMemory, 0, bufferSize, 0, &data);
    std::memcpy(data, indices.data(), bufferSize);
    vkUnmapMemory(m_context.getDevice(), stagingMemory);

    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 m_indexBuffer, m_indexBufferMemory);

    copyBuffer(stagingBuffer, m_indexBuffer, bufferSize);

    vkDestroyBuffer(m_context.getDevice(), stagingBuffer, nullptr);
    vkFreeMemory(m_context.getDevice(), stagingMemory, nullptr);

    m_indexCount = static_cast<u32>(indices.size());
}

void VulkanBuffer::createUniformBuffers(u32 count) {
    m_uniformBuffers.resize(count);
    m_uniformBuffersMemory.resize(count);
    m_uniformBuffersMapped.resize(count);

    VkDeviceSize bufferSize = sizeof(UniformBufferObject);

    for (u32 i = 0; i < count; i++) {
        createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     m_uniformBuffers[i], m_uniformBuffersMemory[i]);

        vkMapMemory(m_context.getDevice(), m_uniformBuffersMemory[i], 0, bufferSize, 0, &m_uniformBuffersMapped[i]);
    }
}

void VulkanBuffer::updateUniformBuffer(u32 index, const UniformBufferObject& ubo) {
    std::memcpy(m_uniformBuffersMapped[index], &ubo, sizeof(ubo));
}

u32 VulkanBuffer::findMemoryType(u32 typeFilter, VkMemoryPropertyFlags properties, VkPhysicalDevice physicalDevice) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (u32 i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    CRF_ASSERT_MSG(false, "Failed to find suitable memory type");
    return 0;
}

VkCommandBuffer VulkanBuffer::beginSingleTimeCommandsInternal() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(m_context.getDevice(), &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    return commandBuffer;
}

void VulkanBuffer::endSingleTimeCommandsInternal(VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(m_context.getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_context.getGraphicsQueue());

    vkFreeCommandBuffers(m_context.getDevice(), m_commandPool, 1, &commandBuffer);
}

}
