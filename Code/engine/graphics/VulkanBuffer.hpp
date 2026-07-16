#pragma once

#include "core/Types.hpp"
#include "VulkanContext.hpp"
#include "Vertex.hpp"
#include <vector>

namespace crf {

struct UniformBufferObject {
    f32 model[16];
    f32 view[16];
    f32 proj[16];
};

class VulkanBuffer {
public:
    VulkanBuffer(VulkanContext& context, VkCommandPool commandPool);
    ~VulkanBuffer();

    VulkanBuffer(const VulkanBuffer&) = delete;
    VulkanBuffer& operator=(const VulkanBuffer&) = delete;
    VulkanBuffer(VulkanBuffer&&) = delete;
    VulkanBuffer& operator=(VulkanBuffer&&) = delete;

    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                      VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

    void createVertexBuffer(const std::vector<Vertex>& vertices);
    void createIndexBuffer(const std::vector<u32>& indices);
    void createUniformBuffers(u32 count);
    void updateUniformBuffer(u32 index, const UniformBufferObject& ubo);

    void destroyBuffers();

    VkBuffer getVertexBuffer() const { return m_vertexBuffer; }
    VkBuffer getIndexBuffer() const { return m_indexBuffer; }
    const std::vector<VkBuffer>& getUniformBuffers() const { return m_uniformBuffers; }
    const std::vector<void*>& getUniformBuffersMapped() const { return m_uniformBuffersMapped; }
    u32 getIndexCount() const { return m_indexCount; }

    static u32 findMemoryType(u32 typeFilter, VkMemoryPropertyFlags properties, VkPhysicalDevice physicalDevice);

private:
    VkCommandBuffer beginSingleTimeCommandsInternal();
    void endSingleTimeCommandsInternal(VkCommandBuffer commandBuffer);

    VulkanContext& m_context;
    VkCommandPool m_commandPool;
    VkBuffer m_vertexBuffer = nullptr;
    VkDeviceMemory m_vertexBufferMemory = nullptr;
    VkBuffer m_indexBuffer = nullptr;
    VkDeviceMemory m_indexBufferMemory = nullptr;
    std::vector<VkBuffer> m_uniformBuffers;
    std::vector<VkDeviceMemory> m_uniformBuffersMemory;
    std::vector<void*> m_uniformBuffersMapped;
    u32 m_indexCount = 0;
};

}
