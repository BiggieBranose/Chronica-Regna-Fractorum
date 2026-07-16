#include "AccelerationStructure.hpp"
#include "VulkanBuffer.hpp"
#include "core/Log.hpp"
#include "core/Assert.hpp"
#include "Vertex.hpp"

#include <vulkan/vulkan.h>
#include <cstring>

namespace crf {

struct RtVertex {
    float pos[4];
    float color[4];
    float uv[4];
};

AccelerationStructure::AccelerationStructure(VulkanContext& context, VkCommandPool commandPool)
    : m_context(context), m_commandPool(commandPool) {
}

AccelerationStructure::~AccelerationStructure() {
    VkDevice device = m_context.getDevice();

    if (m_bottomLevelAS) m_context.vkDestroyAccelerationStructureKHR(device, m_bottomLevelAS, nullptr);
    if (m_topLevelAS) m_context.vkDestroyAccelerationStructureKHR(device, m_topLevelAS, nullptr);

    if (m_bottomLevelASBuffer) {
        vkDestroyBuffer(device, m_bottomLevelASBuffer, nullptr);
        vkFreeMemory(device, m_bottomLevelASMemory, nullptr);
    }
    if (m_topLevelASBuffer) {
        vkDestroyBuffer(device, m_topLevelASBuffer, nullptr);
        vkFreeMemory(device, m_topLevelASMemory, nullptr);
    }
    if (m_scratchBuffer) {
        vkDestroyBuffer(device, m_scratchBuffer, nullptr);
        vkFreeMemory(device, m_scratchBufferMemory, nullptr);
    }
    if (m_instancesBuffer) {
        vkDestroyBuffer(device, m_instancesBuffer, nullptr);
        vkFreeMemory(device, m_instancesBufferMemory, nullptr);
    }
    if (m_vertexBuffer) {
        vkDestroyBuffer(device, m_vertexBuffer, nullptr);
        vkFreeMemory(device, m_vertexBufferMemory, nullptr);
    }
    if (m_rtVertexBuffer) {
        vkDestroyBuffer(device, m_rtVertexBuffer, nullptr);
        vkFreeMemory(device, m_rtVertexBufferMemory, nullptr);
    }
    if (m_indexBuffer) {
        vkDestroyBuffer(device, m_indexBuffer, nullptr);
        vkFreeMemory(device, m_indexBufferMemory, nullptr);
    }
}

void AccelerationStructure::buildBottomLevelAccelerationStructure(const std::vector<Vertex>& vertices, const std::vector<u32>& indices) {
    VkDevice device = m_context.getDevice();

    VkDeviceSize vertexBufferSize = sizeof(Vertex) * vertices.size();

    {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = vertexBufferSize;
        bufferInfo.usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        vkCreateBuffer(device, &bufferInfo, nullptr, &m_vertexBuffer);

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device, m_vertexBuffer, &memRequirements);

        VkMemoryAllocateFlagsInfo allocFlagsInfo{};
        allocFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
        allocFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = VulkanBuffer::findMemoryType(
            memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            m_context.getPhysicalDevice()
        );
        allocInfo.pNext = &allocFlagsInfo;

        vkAllocateMemory(device, &allocInfo, nullptr, &m_vertexBufferMemory);
        vkBindBufferMemory(device, m_vertexBuffer, m_vertexBufferMemory, 0);

        void* data;
        vkMapMemory(device, m_vertexBufferMemory, 0, vertexBufferSize, 0, &data);
        std::memcpy(data, vertices.data(), vertexBufferSize);
        vkUnmapMemory(device, m_vertexBufferMemory);
    }

    VkDeviceSize indexBufferSize = sizeof(u32) * indices.size();

    {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = indexBufferSize;
        bufferInfo.usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        vkCreateBuffer(device, &bufferInfo, nullptr, &m_indexBuffer);

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device, m_indexBuffer, &memRequirements);

        VkMemoryAllocateFlagsInfo allocFlagsInfo{};
        allocFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
        allocFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = VulkanBuffer::findMemoryType(
            memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            m_context.getPhysicalDevice()
        );
        allocInfo.pNext = &allocFlagsInfo;

        vkAllocateMemory(device, &allocInfo, nullptr, &m_indexBufferMemory);
        vkBindBufferMemory(device, m_indexBuffer, m_indexBufferMemory, 0);

        void* data;
        vkMapMemory(device, m_indexBufferMemory, 0, indexBufferSize, 0, &data);
        std::memcpy(data, indices.data(), indexBufferSize);
        vkUnmapMemory(device, m_indexBufferMemory);
    }

    // Create RT vertex buffer (3 vec4 per vertex for closest-hit shader)
    {
        std::vector<RtVertex> rtVertices;
        rtVertices.reserve(vertices.size());
        for (const auto& v : vertices) {
            RtVertex rv{};
            rv.pos[0] = v.pos[0];
            rv.pos[1] = v.pos[1];
            rv.pos[2] = v.pos[2];
            rv.pos[3] = 1.0f;
            rv.color[0] = v.color[0];
            rv.color[1] = v.color[1];
            rv.color[2] = v.color[2];
            rv.color[3] = 1.0f;
            rv.uv[0] = v.texCoord[0];
            rv.uv[1] = v.texCoord[1];
            rv.uv[2] = 0.0f;
            rv.uv[3] = 0.0f;
            rtVertices.push_back(rv);
        }

        VkDeviceSize rtVertexBufferSize = sizeof(RtVertex) * rtVertices.size();

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = rtVertexBufferSize;
        bufferInfo.usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        vkCreateBuffer(device, &bufferInfo, nullptr, &m_rtVertexBuffer);

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device, m_rtVertexBuffer, &memRequirements);

        VkMemoryAllocateFlagsInfo allocFlagsInfo{};
        allocFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
        allocFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = VulkanBuffer::findMemoryType(
            memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            m_context.getPhysicalDevice()
        );
        allocInfo.pNext = &allocFlagsInfo;

        vkAllocateMemory(device, &allocInfo, nullptr, &m_rtVertexBufferMemory);
        vkBindBufferMemory(device, m_rtVertexBuffer, m_rtVertexBufferMemory, 0);

        void* data;
        vkMapMemory(device, m_rtVertexBufferMemory, 0, rtVertexBufferSize, 0, &data);
        std::memcpy(data, rtVertices.data(), rtVertexBufferSize);
        vkUnmapMemory(device, m_rtVertexBufferMemory);
    }

    m_indexCount = static_cast<u32>(indices.size());

    VkBufferDeviceAddressInfo vertexBufferAddressInfo{};
    vertexBufferAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    vertexBufferAddressInfo.buffer = m_vertexBuffer;

    VkBufferDeviceAddressInfo indexBufferAddressInfo{};
    indexBufferAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    indexBufferAddressInfo.buffer = m_indexBuffer;

    VkAccelerationStructureGeometryKHR accelerationStructureGeometry{};
    accelerationStructureGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    accelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    accelerationStructureGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    accelerationStructureGeometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    accelerationStructureGeometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    accelerationStructureGeometry.geometry.triangles.vertexData.deviceAddress = m_context.vkGetBufferDeviceAddress(device, &vertexBufferAddressInfo);
    accelerationStructureGeometry.geometry.triangles.vertexStride = sizeof(Vertex);
    accelerationStructureGeometry.geometry.triangles.maxVertex = static_cast<u32>(vertices.size() - 1);
    accelerationStructureGeometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
    accelerationStructureGeometry.geometry.triangles.indexData.deviceAddress = m_context.vkGetBufferDeviceAddress(device, &indexBufferAddressInfo);

    VkAccelerationStructureBuildGeometryInfoKHR buildGeometryInfo{};
    buildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    buildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    buildGeometryInfo.geometryCount = 1;
    buildGeometryInfo.pGeometries = &accelerationStructureGeometry;

    u32 primitiveCount = static_cast<u32>(indices.size() / 3);

    VkAccelerationStructureBuildSizesInfoKHR buildSizesInfo{};
    buildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

    m_context.vkGetAccelerationStructureBuildSizesKHR(
        device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildGeometryInfo, &primitiveCount, &buildSizesInfo
    );

    createAccelerationStructure(VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
                                1, &buildGeometryInfo, &primitiveCount, m_bottomLevelAS);

    buildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildGeometryInfo.dstAccelerationStructure = m_bottomLevelAS;

    createBuffer(buildSizesInfo.buildScratchSize,
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 m_scratchBuffer, m_scratchBufferMemory);

    VkBufferDeviceAddressInfo scratchAddressInfo{};
    scratchAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    scratchAddressInfo.buffer = m_scratchBuffer;

    buildGeometryInfo.scratchData.deviceAddress = m_context.vkGetBufferDeviceAddress(device, &scratchAddressInfo);

    VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo{};
    buildRangeInfo.primitiveCount = primitiveCount;

    const VkAccelerationStructureBuildRangeInfoKHR* pBuildRangeInfo = &buildRangeInfo;

    VkCommandBuffer commandBuffer;

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_commandPool;
    allocInfo.commandBufferCount = 1;

    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    m_context.vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &buildGeometryInfo, &pBuildRangeInfo);

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(m_context.getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_context.getGraphicsQueue());

    vkFreeCommandBuffers(device, m_commandPool, 1, &commandBuffer);

    if (m_scratchBuffer) {
        vkDestroyBuffer(device, m_scratchBuffer, nullptr);
        vkFreeMemory(device, m_scratchBufferMemory, nullptr);
        m_scratchBuffer = nullptr;
    }

    Log::info("Bottom-level acceleration structure built");
}

void AccelerationStructure::buildTopLevelAccelerationStructure(u32 instanceCount) {
    VkDevice device = m_context.getDevice();

    VkAccelerationStructureGeometryKHR accelerationStructureGeometry{};
    accelerationStructureGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    accelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    accelerationStructureGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    accelerationStructureGeometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;

    VkAccelerationStructureBuildGeometryInfoKHR buildGeometryInfo{};
    buildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    buildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    buildGeometryInfo.geometryCount = 1;
    buildGeometryInfo.pGeometries = &accelerationStructureGeometry;

    VkAccelerationStructureBuildSizesInfoKHR buildSizesInfo{};
    buildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

    m_context.vkGetAccelerationStructureBuildSizesKHR(
        device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildGeometryInfo, &instanceCount, &buildSizesInfo
    );

    createAccelerationStructure(VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
                                1, &buildGeometryInfo, &instanceCount, m_topLevelAS);

    VkBuffer scratchBuffer;
    VkDeviceMemory scratchBufferMemory;
    createBuffer(buildSizesInfo.buildScratchSize,
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 scratchBuffer, scratchBufferMemory);

    buildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildGeometryInfo.dstAccelerationStructure = m_topLevelAS;

    VkBufferDeviceAddressInfo scratchAddressInfo{};
    scratchAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    scratchAddressInfo.buffer = scratchBuffer;

    buildGeometryInfo.scratchData.deviceAddress = m_context.vkGetBufferDeviceAddress(device, &scratchAddressInfo);

    VkAccelerationStructureInstanceKHR instance{};
    instance.transform.matrix[0][0] = 1.0f;
    instance.transform.matrix[1][1] = 1.0f;
    instance.transform.matrix[2][2] = 1.0f;
    instance.instanceCustomIndex = 0;
    instance.mask = 0xFF;
    instance.instanceShaderBindingTableRecordOffset = 0;
    instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;

    VkBufferDeviceAddressInfo instanceBufferAddressInfo{};
    instanceBufferAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;

    createBuffer(sizeof(VkAccelerationStructureInstanceKHR),
                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 m_instancesBuffer, m_instancesBufferMemory);

    void* data;
    vkMapMemory(device, m_instancesBufferMemory, 0, sizeof(VkAccelerationStructureInstanceKHR), 0, &data);
    std::memcpy(data, &instance, sizeof(VkAccelerationStructureInstanceKHR));
    vkUnmapMemory(device, m_instancesBufferMemory);

    instanceBufferAddressInfo.buffer = m_instancesBuffer;

    accelerationStructureGeometry.geometry.instances.data.deviceAddress = m_context.vkGetBufferDeviceAddress(device, &instanceBufferAddressInfo);

    VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo{};
    buildRangeInfo.primitiveCount = instanceCount;

    const VkAccelerationStructureBuildRangeInfoKHR* pBuildRangeInfo = &buildRangeInfo;

    VkCommandBuffer commandBuffer;

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_commandPool;
    allocInfo.commandBufferCount = 1;

    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    m_context.vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &buildGeometryInfo, &pBuildRangeInfo);

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(m_context.getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_context.getGraphicsQueue());

    vkFreeCommandBuffers(device, m_commandPool, 1, &commandBuffer);

    vkDestroyBuffer(device, scratchBuffer, nullptr);
    vkFreeMemory(device, scratchBufferMemory, nullptr);

    Log::info("Top-level acceleration structure built");
}

void AccelerationStructure::createAccelerationStructure(VkAccelerationStructureTypeKHR type, u32 buildGeometryInfoCount,
                                                        const VkAccelerationStructureBuildGeometryInfoKHR* buildGeometryInfo,
                                                        const u32* maxPrimitiveCounts, VkAccelerationStructureKHR& accelerationStructure) {
    (void)buildGeometryInfoCount;
    VkDevice device = m_context.getDevice();

    VkAccelerationStructureBuildSizesInfoKHR buildSizesInfo{};
    buildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

    m_context.vkGetAccelerationStructureBuildSizesKHR(
        device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        buildGeometryInfo, maxPrimitiveCounts, &buildSizesInfo
    );

    VkBuffer asBuffer;
    VkDeviceMemory asBufferMemory;

    createBuffer(buildSizesInfo.accelerationStructureSize,
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 asBuffer, asBufferMemory);

    VkAccelerationStructureCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    createInfo.buffer = asBuffer;
    createInfo.offset = 0;
    createInfo.size = buildSizesInfo.accelerationStructureSize;
    createInfo.type = type;

    VkResult result = m_context.vkCreateAccelerationStructureKHR(device, &createInfo, nullptr, &accelerationStructure);
    CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to create acceleration structure");

    if (type == VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR) {
        m_bottomLevelASBuffer = asBuffer;
        m_bottomLevelASMemory = asBufferMemory;
    } else {
        m_topLevelASBuffer = asBuffer;
        m_topLevelASMemory = asBufferMemory;
    }
}

void AccelerationStructure::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
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

    VkMemoryAllocateFlagsInfo allocFlagsInfo{};
    allocFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
    bool needsDeviceAddress = (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) != 0;
    if (needsDeviceAddress) {
        allocFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
    }

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = VulkanBuffer::findMemoryType(memRequirements.memoryTypeBits, properties, m_context.getPhysicalDevice());
    if (needsDeviceAddress) {
        allocInfo.pNext = &allocFlagsInfo;
    }

    result = vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory);
    CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to allocate buffer memory");

    vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

u32 AccelerationStructure::getAlignedSize(u32 originalSize, u32 alignment) {
    return (originalSize + alignment - 1) & ~(alignment - 1);
}

}
