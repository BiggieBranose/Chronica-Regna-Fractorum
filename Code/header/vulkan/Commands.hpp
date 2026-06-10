#pragma once

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <vector>

namespace vkapp
{
    class VulkanDevice;
    class VulkanInstance;
    class SwapchainPipeline;
    class Buffers;

    class Commands
    {
    public:
        Commands() = default;
        ~Commands() = default;

        Commands(const Commands&) = delete;
        Commands& operator=(const Commands&) = delete;

        void initialize(
            VulkanInstance& instance,
            VulkanDevice& device,
            SwapchainPipeline& pipeline,
            Buffers& buffers);

        void cleanup(VulkanDevice& device);

        void updateUniformBuffer(VulkanDevice& device, SwapchainPipeline& pipeline, Buffers& buffers);

        void drawFrame(
            VulkanInstance& instance,
            VulkanDevice& device,
            SwapchainPipeline& pipeline,
            Buffers& buffers,
            bool& framebufferResized);

        vk::raii::CommandPool& getCommandPool() { return m_commandPool; }

    private:
        void createCommandPool(VulkanDevice& device);
        void createCommandBuffers(VulkanDevice& device);
        void createSyncObjects(VulkanDevice& device, uint32_t swapchainImageCount);

        void recordCommandBuffer(
            vk::CommandBuffer cb,
            uint32_t imageIndex,
            VulkanDevice& device,
            SwapchainPipeline& pipeline,
            Buffers& buffers);

    private:
        vk::raii::CommandPool m_commandPool = nullptr;
        std::vector<vk::raii::CommandBuffer> m_commandBuffers;

        std::vector<vk::raii::Semaphore> m_imageAvailable;
        std::vector<vk::raii::Semaphore> m_renderFinished;
        std::vector<vk::raii::Fence>     m_inFlight;

        uint32_t m_frameIndex = 0;
    };
}
