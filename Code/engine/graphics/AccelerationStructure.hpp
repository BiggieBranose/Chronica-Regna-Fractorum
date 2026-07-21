#pragma once

#include "core/Types.hpp"
#include "VulkanContext.hpp"
#include "Vertex.hpp"
#include <vector>

struct VkAccelerationStructureKHR_T;
using VkAccelerationStructureKHR = VkAccelerationStructureKHR_T*;

namespace crf {

class AccelerationStructure {
public:
    AccelerationStructure(VulkanContext& context, VkCommandPool commandPool);
    ~AccelerationStructure();

    AccelerationStructure(const AccelerationStructure&) = delete;
    AccelerationStructure& operator=(const AccelerationStructure&) = delete;
    AccelerationStructure(AccelerationStructure&&) = delete;
    AccelerationStructure& operator=(AccelerationStructure&&) = delete;

    void buildBottomLevelAccelerationStructure(const std::vector<Vertex>& vertices, const std::vector<u32>& indices);
    void buildTopLevelAccelerationStructure(u32 instanceCount);

VkAccelerationStructureKHR getBottomLevelAS() const { return m_bottomLevelAS; }
    VkAccelerationStructureKHR getTopLevelAS() const { return m_topLevelAS; }
    VkBuffer getInstancesBuffer() const { return m_instancesBuffer; }
    VkBuffer getVertexBuffer() const { return m_vertexBuffer; }
    VkBuffer getRtVertexBuffer() const { return m_rtVertexBuffer; }
    VkBuffer getIndexBuffer() const { return m_indexBuffer; }
    u32 getIndexCount() const { return m_indexCount; }

private:
    void createAccelerationStructure(VkAccelerationStructureTypeKHR type, u32 buildGeometryInfoCount,
                                     const VkAccelerationStructureBuildGeometryInfoKHR* buildGeometryInfo,
                                     const u32* maxPrimitiveCounts, VkAccelerationStructureKHR& accelerationStructure);
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                      VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    u32 getAlignedSize(u32 originalSize, u32 alignment);

    VulkanContext& m_context;
    VkCommandPool m_commandPool;
    VkAccelerationStructureKHR m_bottomLevelAS = nullptr;
    VkAccelerationStructureKHR m_topLevelAS = nullptr;
    VkBuffer m_bottomLevelASBuffer = nullptr;
    VkDeviceMemory m_bottomLevelASMemory = nullptr;
    VkBuffer m_topLevelASBuffer = nullptr;
    VkDeviceMemory m_topLevelASMemory = nullptr;
    VkBuffer m_scratchBuffer = nullptr;
    VkDeviceMemory m_scratchBufferMemory = nullptr;
VkBuffer m_instancesBuffer = nullptr;
    VkDeviceMemory m_instancesBufferMemory = nullptr;
    VkBuffer m_vertexBuffer = nullptr;
    VkDeviceMemory m_vertexBufferMemory = nullptr;
    VkBuffer m_blasVertexBuffer = nullptr;
    VkDeviceMemory m_blasVertexBufferMemory = nullptr;
    VkBuffer m_rtVertexBuffer = nullptr;
    VkDeviceMemory m_rtVertexBufferMemory = nullptr;
    VkBuffer m_indexBuffer = nullptr;
    VkDeviceMemory m_indexBufferMemory = nullptr;
    u32 m_indexCount = 0;
};

}
