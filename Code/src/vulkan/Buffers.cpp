#include "../../header/vulkan/Buffers.hpp"
#include "../../header/vulkan/Device.hpp"
#include "../../header/vulkan/SwapchainPipeline.hpp"

#include <cstring>
#include <stdexcept>

namespace vkapp
{
    // ----------------- Vertex layout -----------------

    vk::VertexInputBindingDescription Vertex::getBindingDescription()
    {
        vk::VertexInputBindingDescription desc{};
        desc.binding   = 0;
        desc.stride    = sizeof(Vertex);
        desc.inputRate = vk::VertexInputRate::eVertex;
        return desc;
    }

    std::array<vk::VertexInputAttributeDescription, 2> Vertex::getAttributeDescriptions()
    {
        std::array<vk::VertexInputAttributeDescription, 2> attrs{};

        attrs[0].location = 0;
        attrs[0].binding  = 0;
        attrs[0].format   = vk::Format::eR32G32Sfloat;
        attrs[0].offset   = offsetof(Vertex, pos);

        attrs[1].location = 1;
        attrs[1].binding  = 0;
        attrs[1].format   = vk::Format::eR32G32B32Sfloat;
        attrs[1].offset   = offsetof(Vertex, color);

        return attrs;
    }

    // ----------------- PUBLIC -----------------

    void Buffers::initialize(VulkanDevice& device, SwapchainPipeline& pipeline)
    {
        uint32_t count = pipeline.getSwapchainImages().size();

        createVertexBuffer(device);
        createIndexBuffer(device);
        createUniformBuffers(device, count);
        createDescriptorPool(device, count);
        createDescriptorSets(device, pipeline, count);
    }

    void Buffers::cleanup(VulkanDevice& device)
    {
        VmaAllocator allocator = device.getAllocator();

        if (m_vertexBuffer != VK_NULL_HANDLE)
        {
            vmaDestroyBuffer(allocator, m_vertexBuffer, m_vertexAlloc);
            m_vertexBuffer = VK_NULL_HANDLE;
        }

        if (m_indexBuffer != VK_NULL_HANDLE)
        {
            vmaDestroyBuffer(allocator, m_indexBuffer, m_indexAlloc);
            m_indexBuffer = VK_NULL_HANDLE;
        }

        for (size_t i = 0; i < m_uniformBuffers.size(); i++)
        {
            if (m_uniformMapped[i] != nullptr)
            {
                vmaUnmapMemory(allocator, m_uniformAllocs[i]);
                m_uniformMapped[i] = nullptr;
            }

            vmaDestroyBuffer(allocator, m_uniformBuffers[i], m_uniformAllocs[i]);
        }

        m_descriptorSets.clear();
        m_descriptorPool = nullptr;
    }

    // ----------------- VERTEX BUFFER -----------------

    void Buffers::createVertexBuffer(VulkanDevice& device)
    {
        const std::vector<Vertex> vertices = {
            {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
            {{ 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
            {{ 0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}},
            {{-0.5f,  0.5f}, {1.0f, 1.0f, 1.0f}}
        };

        VkBufferCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        info.size  = sizeof(vertices[0]) * vertices.size();
        info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

        VmaAllocationCreateInfo alloc{};
        alloc.usage = VMA_MEMORY_USAGE_AUTO;
        alloc.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

        if (vmaCreateBuffer(device.getAllocator(), &info, &alloc,
                            &m_vertexBuffer, &m_vertexAlloc, nullptr) != VK_SUCCESS)
            throw std::runtime_error("failed to create vertex buffer");

        void* data = nullptr;
        vmaMapMemory(device.getAllocator(), m_vertexAlloc, &data);
        std::memcpy(data, vertices.data(), static_cast<size_t>(info.size));
        vmaUnmapMemory(device.getAllocator(), m_vertexAlloc);
    }

    // ----------------- INDEX BUFFER -----------------

    void Buffers::createIndexBuffer(VulkanDevice& device)
    {
        const std::vector<uint16_t> indices = { 0, 1, 2, 2, 3, 0 };

        VkBufferCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        info.size  = sizeof(indices[0]) * indices.size();
        info.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

        VmaAllocationCreateInfo alloc{};
        alloc.usage = VMA_MEMORY_USAGE_AUTO;
        alloc.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

        if (vmaCreateBuffer(device.getAllocator(), &info, &alloc,
                            &m_indexBuffer, &m_indexAlloc, nullptr) != VK_SUCCESS)
            throw std::runtime_error("failed to create index buffer");

        void* data = nullptr;
        vmaMapMemory(device.getAllocator(), m_indexAlloc, &data);
        std::memcpy(data, indices.data(), static_cast<size_t>(info.size));
        vmaUnmapMemory(device.getAllocator(), m_indexAlloc);
    }

    // ----------------- UNIFORM BUFFERS -----------------

    void Buffers::createUniformBuffers(VulkanDevice& device, uint32_t count)
    {
        m_uniformBuffers.resize(count);
        m_uniformAllocs.resize(count);
        m_uniformMapped.resize(count);

        VkDeviceSize size = sizeof(UniformBufferObject);

        for (uint32_t i = 0; i < count; i++)
        {
            VkBufferCreateInfo info{};
            info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            info.size  = size;
            info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

            VmaAllocationCreateInfo alloc{};
            alloc.usage = VMA_MEMORY_USAGE_AUTO;
            alloc.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                        VMA_ALLOCATION_CREATE_MAPPED_BIT;

            VmaAllocationInfo ainfo{};

            if (vmaCreateBuffer(device.getAllocator(), &info, &alloc,
                                &m_uniformBuffers[i], &m_uniformAllocs[i], &ainfo) != VK_SUCCESS)
                throw std::runtime_error("failed to create uniform buffer");

            m_uniformMapped[i] = ainfo.pMappedData;
        }
    }

    // ----------------- DESCRIPTOR POOL -----------------

    void Buffers::createDescriptorPool(VulkanDevice& device, uint32_t count)
    {
        vk::DescriptorPoolSize pool{};
        pool.type            = vk::DescriptorType::eUniformBuffer;
        pool.descriptorCount = count;

        vk::DescriptorPoolCreateInfo info{};
        info.poolSizeCount = 1;
        info.pPoolSizes    = &pool;
        info.maxSets       = count;

        m_descriptorPool = vk::raii::DescriptorPool(device.getDevice(), info);
    }

    // ----------------- DESCRIPTOR SETS -----------------

    void Buffers::createDescriptorSets(
        VulkanDevice& device,
        SwapchainPipeline& pipeline,
        uint32_t count)
    {
        std::vector<vk::DescriptorSetLayout> layouts(count, *pipeline.getDescriptorSetLayout());

        vk::DescriptorSetAllocateInfo alloc{};
        alloc.descriptorPool     = *m_descriptorPool;
        alloc.descriptorSetCount = count;
        alloc.pSetLayouts        = layouts.data();

        m_descriptorSets = vk::raii::DescriptorSets(device.getDevice(), alloc);

        for (uint32_t i = 0; i < count; i++)
        {
            vk::DescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = m_uniformBuffers[i];
            bufferInfo.offset = 0;
            bufferInfo.range  = sizeof(UniformBufferObject);

            vk::WriteDescriptorSet write{};
            write.dstSet          = *m_descriptorSets[i];
            write.dstBinding      = 0;
            write.dstArrayElement = 0;
            write.descriptorType  = vk::DescriptorType::eUniformBuffer;
            write.descriptorCount = 1;
            write.pBufferInfo     = &bufferInfo;

            device.getDevice().updateDescriptorSets(write, nullptr);
        }
    }
    void Buffers::createBuffer(VulkanDevice& device, vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::raii::Buffer& buffer, vk::raii::DeviceMemory& bufferMemory) {
        vk::BufferCreateInfo bufferInfo( {}, size, usage, vk::SharingMode::eExclusive );
        buffer = vk::raii::Buffer(device.getDevice(), bufferInfo);
        vk::MemoryRequirements memRequirements = buffer.getMemoryRequirements();
        vk::MemoryAllocateInfo allocInfo( memRequirements.size, findMemoryType(device, memRequirements.memoryTypeBits, properties), nullptr );
        bufferMemory = vk::raii::DeviceMemory(device.getDevice(), allocInfo);
        buffer.bindMemory(*bufferMemory, 0);
    }

    uint32_t findMemoryType(VulkanDevice& device, uint32_t typeFilter, vk::MemoryPropertyFlags properties){
        vk::PhysicalDeviceMemoryProperties memProperties = device.getPhysicalDevice().getMemoryProperties();
        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
        {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
            {
                return i;
            }
        }

        throw std::runtime_error("failed to find suitable memory type!");
    }

}
