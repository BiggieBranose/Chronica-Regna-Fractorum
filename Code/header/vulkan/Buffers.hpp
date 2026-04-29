#pragma once

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>
#include "../../external/VMA/vk_mem_alloc.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <vector>

namespace vkapp
{
    class VulkanDevice;
    class SwapchainPipeline;

    struct Vertex
    {
        glm::vec3 pos;
        glm::vec3 color;
        glm::vec2 texCoord;

        static vk::VertexInputBindingDescription getBindingDescription();
        static std::array<vk::VertexInputAttributeDescription, 3> getAttributeDescriptions();
    };

    struct UniformBufferObject
    {
        alignas(16) glm::mat4 model;
        alignas(16) glm::mat4 view;
        alignas(16) glm::mat4 proj;
    };

    class Buffers
    {
    public:
        Buffers() = default;
        ~Buffers() = default;

        Buffers(const Buffers&) = delete;
        Buffers& operator=(const Buffers&) = delete;

        void initialize(VulkanDevice& device, SwapchainPipeline& pipeline);
        void cleanup(VulkanDevice& device);

        // Accessors
        VkBuffer getVertexBuffer() const { return m_vertexBuffer; }
        VkBuffer getIndexBuffer()  const { return m_indexBuffer; }

        const std::vector<VkBuffer>& getUniformBuffers() const { return m_uniformBuffers; }
        const std::vector<void*>&    getUniformMapped()   const { return m_uniformMapped; }

        vk::raii::DescriptorPool& getDescriptorPool() { return m_descriptorPool; }
        const std::vector<vk::raii::DescriptorSet>& getDescriptorSets() const { return m_descriptorSets; }

    private:
        void createVertexBuffer(VulkanDevice& device);
        void createIndexBuffer(VulkanDevice& device);
        void createUniformBuffers(VulkanDevice& device, uint32_t swapchainImageCount);

        void createDescriptorPool(VulkanDevice& device, uint32_t swapchainImageCount);
        void createDescriptorSets(VulkanDevice& device, SwapchainPipeline& pipeline, uint32_t swapchainImageCount);

    private:
        // Vertex + index buffers
        VkBuffer       m_vertexBuffer = VK_NULL_HANDLE;
        VmaAllocation  m_vertexAlloc  = VK_NULL_HANDLE;

        VkBuffer       m_indexBuffer  = VK_NULL_HANDLE;
        VmaAllocation  m_indexAlloc   = VK_NULL_HANDLE;

        // Uniform buffers
        std::vector<VkBuffer>      m_uniformBuffers;
        std::vector<VmaAllocation> m_uniformAllocs;
        std::vector<void*>         m_uniformMapped;

        // Descriptor sets
        vk::raii::DescriptorPool              m_descriptorPool = nullptr;
        std::vector<vk::raii::DescriptorSet>  m_descriptorSets;
    };
}
