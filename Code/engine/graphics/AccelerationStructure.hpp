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
};

}
