#pragma once

#include "core/Types.hpp"
#include "VulkanContext.hpp"
#include <vector>
#include <functional>

namespace crf {

struct FramebufferAttachment {
    VkImage image = nullptr;
    VkDeviceMemory memory = nullptr;
    VkImageView view = nullptr;
};

class VulkanRenderPass {
public:
    VulkanRenderPass(VulkanContext& context);
    ~VulkanRenderPass();

    VulkanRenderPass(const VulkanRenderPass&) = delete;
    VulkanRenderPass& operator=(const VulkanRenderPass&) = delete;
    VulkanRenderPass(VulkanRenderPass&&) = delete;
    VulkanRenderPass& operator=(VulkanRenderPass&&) = delete;

    void createRenderPass();
    void createFramebuffers();
    void createCommandPool();
    void createCommandBuffers();
    void createSyncObjects();
    void createColorResources();
    void createDepthResources();
    void cleanupSwapChain();

    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);

    bool drawFrame(std::function<void(VkCommandBuffer)> recordCallback);
    void waitForFences();
    void resetFences();

    VkRenderPass getRenderPass() const { return m_renderPass; }
    VkCommandBuffer getCommandBuffer(u32 index) const { return m_commandBuffers[index]; }
    VkCommandPool getCommandPool() const { return m_commandPool; }
    u32 getCurrentFrame() const { return m_currentFrame; }
    bool wasFramebufferResized() const { return m_framebufferResized; }
    void setFramebufferResized(bool resized) { m_framebufferResized = resized; }
    VkSampleCountFlagBits getMsaaSamples() const { return m_msaaSamples; }

private:
    void createImage(u32 width, u32 height, u32 mipLevels, VkSampleCountFlagBits numSamples,
                     VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
                     VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, u32 mipLevels);
    VkSampleCountFlagBits getMaxUsableSampleCount() const;

    VulkanContext& m_context;
    VkRenderPass m_renderPass = nullptr;
    std::vector<VkFramebuffer> m_swapChainFramebuffers;
    VkCommandPool m_commandPool = nullptr;
    std::vector<VkCommandBuffer> m_commandBuffers;
    std::vector<VkSemaphore> m_imageAvailableSemaphores;
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
    std::vector<VkFence> m_inFlightFences;
    std::vector<VkFence> m_imagesInFlight;
    std::vector<VkSemaphore> m_perImageSemaphores;
    u32 m_currentFrame = 0;
    bool m_framebufferResized = false;
    VkSampleCountFlagBits m_msaaSamples = VK_SAMPLE_COUNT_1_BIT;
    FramebufferAttachment m_colorAttachment{};
    FramebufferAttachment m_depthAttachment{};
};

}
