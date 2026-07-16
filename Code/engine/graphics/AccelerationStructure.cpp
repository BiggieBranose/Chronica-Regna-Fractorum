#include "AccelerationStructure.hpp"
#include "VulkanBuffer.hpp"
#include "core/Log.hpp"
#include "core/Assert.hpp"

#include <vulkan/vulkan.h>
#include <cstring>

namespace crf {

AccelerationStructure::AccelerationStructure(VulkanContext& context, VkCommandPool commandPool)
    : m_context(context), m_commandPool(commandPool) {
}

AccelerationStructure::~AccelerationStructure() {
    VkDevice device = m_context.getDevice();

    if (m_bottomLevelAS) vkDestroyAccelerationStructureKHR(device, m_bottomLevelAS, nullptr);
    if (m_topLevelAS) vkDestroyAccelerationStructureKHR(device, m_topLevelAS, nullptr);

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
}

void AccelerationStructure::buildBottomLevelAccelerationStructure(const std::vector<Vertex>& vertices, const std::vector<u32>& indices) {
    VkDevice device = m_context.getDevice();

    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    VkDeviceSize vertexBufferSize = sizeof(Vertex) * vertices.size();

    {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = vertexBufferSize;
        bufferInfo.usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        vkCreateBuffer(device, &bufferInfo, nullptr, &vertexBuffer);

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device, vertexBuffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = VulkanBuffer::findMemoryType(
            memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            m_context.getPhysicalDevice()
        );

        vkAllocateMemory(device, &allocInfo, nullptr, &vertexBufferMemory);
        vkBindBufferMemory(device, vertexBuffer, vertexBufferMemory, 0);

        void* data;
        vkMapMemory(device, vertexBufferMemory, 0, vertexBufferSize, 0, &data);
        std::memcpy(data, vertices.data(), vertexBufferSize);
        vkUnmapMemory(device, vertexBufferMemory);
    }

    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;
    VkDeviceSize indexBufferSize = sizeof(u32) * indices.size();

    {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = indexBufferSize;
        bufferInfo.usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        vkCreateBuffer(device, &bufferInfo, nullptr, &indexBuffer);

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device, indexBuffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = VulkanBuffer::findMemoryType(
            memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            m_context.getPhysicalDevice()
        );

        vkAllocateMemory(device, &allocInfo, nullptr, &indexBufferMemory);
        vkBindBufferMemory(device, indexBuffer, indexBufferMemory, 0);

        void* data;
        vkMapMemory(device, indexBufferMemory, 0, indexBufferSize, 0, &data);
        std::memcpy(data, indices.data(), indexBufferSize);
        vkUnmapMemory(device, indexBufferMemory);
    }

    VkBufferDeviceAddressInfo vertexBufferAddressInfo{};
    vertexBufferAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    vertexBufferAddressInfo.buffer = vertexBuffer;

    VkBufferDeviceAddressInfo indexBufferAddressInfo{};
    indexBufferAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    indexBufferAddressInfo.buffer = indexBuffer;

    VkAccelerationStructureGeometryKHR accelerationStructureGeometry{};
    accelerationStructureGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    accelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    accelerationStructureGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    accelerationStructureGeometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    accelerationStructureGeometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    accelerationStructureGeometry.geometry.triangles.vertexData.deviceAddress = vkGetBufferDeviceAddress(device, &vertexBufferAddressInfo);
    accelerationStructureGeometry.geometry.triangles.vertexStride = sizeof(Vertex);
    accelerationStructureGeometry.geometry.triangles.maxVertex = static_cast<u32>(vertices.size() - 1);
    accelerationStructureGeometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
    accelerationStructureGeometry.geometry.triangles.indexData.deviceAddress = vkGetBufferDeviceAddress(device, &indexBufferAddressInfo);

    VkAccelerationStructureBuildGeometryInfoKHR buildGeometryInfo{};
    buildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    buildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    buildGeometryInfo.geometryCount = 1;
    buildGeometryInfo.pGeometries = &accelerationStructureGeometry;

    u32 primitiveCount = static_cast<u32>(indices.size() / 3);

    VkAccelerationStructureBuildSizesInfoKHR buildSizesInfo{};
    buildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

    vkGetAccelerationStructureBuildSizesKHR(
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

    buildGeometryInfo.scratchData.deviceAddress = vkGetBufferDeviceAddress(device, &scratchAddressInfo);

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

    vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &buildGeometryInfo, &pBuildRangeInfo);

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

    vkDestroyBuffer(device, vertexBuffer, nullptr);
    vkFreeMemory(device, vertexBufferMemory, nullptr);
    vkDestroyBuffer(device, indexBuffer, nullptr);
    vkFreeMemory(device, indexBufferMemory, nullptr);

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

    vkGetAccelerationStructureBuildSizesKHR(
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

    buildGeometryInfo.scratchData.deviceAddress = vkGetBufferDeviceAddress(device, &scratchAddressInfo);

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

    accelerationStructureGeometry.geometry.instances.data.deviceAddress = vkGetBufferDeviceAddress(device, &instanceBufferAddressInfo);

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

    vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &buildGeometryInfo, &pBuildRangeInfo);

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

    vkGetAccelerationStructureBuildSizesKHR(
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

    VkResult result = vkCreateAccelerationStructureKHR(device, &createInfo, nullptr, &accelerationStructure);
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

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = VulkanBuffer::findMemoryType(memRequirements.memoryTypeBits, properties, m_context.getPhysicalDevice());

    result = vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory);
    CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to allocate buffer memory");

    vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

u32 AccelerationStructure::getAlignedSize(u32 originalSize, u32 alignment) {
    return (originalSize + alignment - 1) & ~(alignment - 1);
}

}
