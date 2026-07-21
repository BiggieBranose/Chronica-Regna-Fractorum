#include "VulkanRenderPass.hpp"
#include "VulkanBuffer.hpp"
#include "core/Log.hpp"
#include "core/Assert.hpp"

#include <array>
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

namespace crf {

VulkanRenderPass::VulkanRenderPass(VulkanContext& context) : m_context(context) {
    m_msaaSamples = context.getPhysicalDevice() ? VK_SAMPLE_COUNT_4_BIT : VK_SAMPLE_COUNT_1_BIT;

    VkPhysicalDeviceProperties physicalDeviceProperties;
    vkGetPhysicalDeviceProperties(context.getPhysicalDevice(), &physicalDeviceProperties);

    VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts &
                                physicalDeviceProperties.limits.framebufferDepthSampleCounts;

    if (counts & VK_SAMPLE_COUNT_4_BIT) m_msaaSamples = VK_SAMPLE_COUNT_4_BIT;
    else if (counts & VK_SAMPLE_COUNT_2_BIT) m_msaaSamples = VK_SAMPLE_COUNT_2_BIT;
    else m_msaaSamples = VK_SAMPLE_COUNT_1_BIT;
}

VulkanRenderPass::~VulkanRenderPass() {
    VkDevice device = m_context.getDevice();

    for (auto framebuffer : m_swapChainFramebuffers) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }

    if (m_colorAttachment.view) {
        vkDestroyImageView(device, m_colorAttachment.view, nullptr);
        vkDestroyImage(device, m_colorAttachment.image, nullptr);
        vkFreeMemory(device, m_colorAttachment.memory, nullptr);
    }

    if (m_depthAttachment.view) {
        vkDestroyImageView(device, m_depthAttachment.view, nullptr);
        vkDestroyImage(device, m_depthAttachment.image, nullptr);
        vkFreeMemory(device, m_depthAttachment.memory, nullptr);
    }

    for (u32 i = 0; i < VulkanContext::MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(device, m_imageAvailableSemaphores[i], nullptr);
        vkDestroySemaphore(device, m_renderFinishedSemaphores[i], nullptr);
        vkDestroyFence(device, m_inFlightFences[i], nullptr);
    }

    for (auto semaphore : m_perImageSemaphores) {
        vkDestroySemaphore(device, semaphore, nullptr);
    }

    vkDestroyCommandPool(device, m_commandPool, nullptr);
    vkDestroyRenderPass(device, m_renderPass, nullptr);
}

void VulkanRenderPass::createRenderPass() {
    VkDevice device = m_context.getDevice();

    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = m_context.getSwapChainImageFormat();
    colorAttachment.samples = m_msaaSamples;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription colorAttachmentResolve{};
    colorAttachmentResolve.format = m_context.getSwapChainImageFormat();
    colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = m_context.findDepthFormat();
    depthAttachment.samples = m_msaaSamples;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentResolveRef{};
    colorAttachmentResolveRef.attachment = 2;
    colorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;
    subpass.pResolveAttachments = &colorAttachmentResolveRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 3> attachments = {colorAttachment, depthAttachment, colorAttachmentResolve};

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<u32>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    VkResult result = vkCreateRenderPass(device, &renderPassInfo, nullptr, &m_renderPass);
    CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to create render pass");
}

void VulkanRenderPass::createFramebuffers() {
    VkDevice device = m_context.getDevice();
    auto& imageViewViews = m_context.getSwapChainImageViews();

    m_swapChainFramebuffers.resize(imageViewViews.size());

    for (size_t i = 0; i < imageViewViews.size(); i++) {
        std::array<VkImageView, 3> attachments = {
            m_colorAttachment.view,
            m_depthAttachment.view,
            imageViewViews[i]
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = m_renderPass;
        framebufferInfo.attachmentCount = static_cast<u32>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = m_context.getSwapChainExtent().width;
        framebufferInfo.height = m_context.getSwapChainExtent().height;
        framebufferInfo.layers = 1;

        VkResult result = vkCreateFramebuffer(device, &framebufferInfo, nullptr, &m_swapChainFramebuffers[i]);
        CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to create framebuffer");
    }
}

void VulkanRenderPass::createCommandPool() {
    VkDevice device = m_context.getDevice();

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    u32 queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_context.getPhysicalDevice(), &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_context.getPhysicalDevice(), &queueFamilyCount, queueFamilies.data());

    for (u32 i = 0; i < queueFamilyCount; i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            poolInfo.queueFamilyIndex = i;
            break;
        }
    }

    VkResult result = vkCreateCommandPool(device, &poolInfo, nullptr, &m_commandPool);
    CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to create command pool");
}

void VulkanRenderPass::createCommandBuffers() {
    VkDevice device = m_context.getDevice();

    m_commandBuffers.resize(VulkanContext::MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<u32>(m_commandBuffers.size());

    VkResult result = vkAllocateCommandBuffers(device, &allocInfo, m_commandBuffers.data());
    CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to allocate command buffers");
}

void VulkanRenderPass::createSyncObjects() {
    VkDevice device = m_context.getDevice();

    m_imageAvailableSemaphores.resize(VulkanContext::MAX_FRAMES_IN_FLIGHT);
    m_renderFinishedSemaphores.resize(VulkanContext::MAX_FRAMES_IN_FLIGHT);
    m_inFlightFences.resize(VulkanContext::MAX_FRAMES_IN_FLIGHT);
    m_imagesInFlight.resize(m_context.getSwapChainImageCount(), VK_NULL_HANDLE);

    m_perImageSemaphores.resize(m_context.getSwapChainImageCount());

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (u32 i = 0; i < VulkanContext::MAX_FRAMES_IN_FLIGHT; i++) {
        VkResult result = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]);
        CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to create image available semaphore");
        result = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]);
        CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to create render finished semaphore");
        result = vkCreateFence(device, &fenceInfo, nullptr, &m_inFlightFences[i]);
        CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to create in-flight fence");
    }

    for (u32 i = 0; i < m_context.getSwapChainImageCount(); i++) {
        VkResult result = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &m_perImageSemaphores[i]);
        CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to create per-image semaphore");
    }
}

void VulkanRenderPass::createColorResources() {
    VkFormat colorFormat = m_context.getSwapChainImageFormat();

    createImage(
        m_context.getSwapChainExtent().width,
        m_context.getSwapChainExtent().height,
        1, m_msaaSamples, colorFormat,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        m_colorAttachment.image, m_colorAttachment.memory
    );

    m_colorAttachment.view = createImageView(
        m_colorAttachment.image, colorFormat,
        VK_IMAGE_ASPECT_COLOR_BIT, 1
    );
}

void VulkanRenderPass::createDepthResources() {
    VkFormat depthFormat = m_context.findDepthFormat();

    createImage(
        m_context.getSwapChainExtent().width,
        m_context.getSwapChainExtent().height,
        1, m_msaaSamples, depthFormat,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        m_depthAttachment.image, m_depthAttachment.memory
    );

    m_depthAttachment.view = createImageView(
        m_depthAttachment.image, depthFormat,
        VK_IMAGE_ASPECT_DEPTH_BIT, 1
    );
}

void VulkanRenderPass::cleanupSwapChain() {
    VkDevice device = m_context.getDevice();

    for (auto framebuffer : m_swapChainFramebuffers) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }
    m_swapChainFramebuffers.clear();

    if (m_colorAttachment.view) {
        vkDestroyImageView(device, m_colorAttachment.view, nullptr);
        vkDestroyImage(device, m_colorAttachment.image, nullptr);
        vkFreeMemory(device, m_colorAttachment.memory, nullptr);
        m_colorAttachment = {};
    }

    if (m_depthAttachment.view) {
        vkDestroyImageView(device, m_depthAttachment.view, nullptr);
        vkDestroyImage(device, m_depthAttachment.image, nullptr);
        vkFreeMemory(device, m_depthAttachment.memory, nullptr);
        m_depthAttachment = {};
    }
}

VkCommandBuffer VulkanRenderPass::beginSingleTimeCommands() {
    VkDevice device = m_context.getDevice();

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    return commandBuffer;
}

void VulkanRenderPass::endSingleTimeCommands(VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(m_context.getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_context.getGraphicsQueue());

    vkFreeCommandBuffers(m_context.getDevice(), m_commandPool, 1, &commandBuffer);
}

bool VulkanRenderPass::drawFrame(std::function<void(VkCommandBuffer, u32 imageIndex)> recordCallback) {
    VkDevice device = m_context.getDevice();

    vkWaitForFences(device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);

    u32 imageIndex;
    VkResult result = vkAcquireNextImageKHR(
        device, m_context.getSwapChain(), UINT64_MAX,
        m_imageAvailableSemaphores[m_currentFrame], VK_NULL_HANDLE, &imageIndex
    );

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        m_context.recreateSwapChain();
        cleanupSwapChain();
        createColorResources();
        createDepthResources();
        createFramebuffers();
        return false;
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        CRF_ASSERT_MSG(false, "Failed to acquire swap chain image");
    }

    if (m_imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
        vkWaitForFences(device, 1, &m_imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
    }

    m_imagesInFlight[imageIndex] = m_inFlightFences[m_currentFrame];

    vkResetFences(device, 1, &m_inFlightFences[m_currentFrame]);

    vkResetCommandBuffer(m_commandBuffers[m_currentFrame], 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    vkBeginCommandBuffer(m_commandBuffers[m_currentFrame], &beginInfo);

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_renderPass;
    renderPassInfo.framebuffer = m_swapChainFramebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = m_context.getSwapChainExtent();
    renderPassInfo.clearValueCount = static_cast<u32>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(m_commandBuffers[m_currentFrame], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    if (recordCallback) {
        recordCallback(m_commandBuffers[m_currentFrame], imageIndex);
    }

    vkCmdEndRenderPass(m_commandBuffers[m_currentFrame]);

    VkResult endResult = vkEndCommandBuffer(m_commandBuffers[m_currentFrame]);
    CRF_ASSERT_MSG(endResult == VK_SUCCESS, "Failed to record command buffer");

    VkSemaphore waitSemaphores[] = {m_imageAvailableSemaphores[m_currentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSemaphore signalSemaphores[] = {m_perImageSemaphores[imageIndex]};

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_commandBuffers[m_currentFrame];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    VkResult submitResult = vkQueueSubmit(m_context.getGraphicsQueue(), 1, &submitInfo, m_inFlightFences[m_currentFrame]);
    CRF_ASSERT_MSG(submitResult == VK_SUCCESS, "Failed to submit draw command buffer");

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = {m_context.getSwapChain()};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(m_context.getPresentQueue(), &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_framebufferResized) {
        m_framebufferResized = false;
        m_context.recreateSwapChain();
        cleanupSwapChain();
        createColorResources();
        createDepthResources();
        createFramebuffers();
    } else if (result != VK_SUCCESS) {
        CRF_ASSERT_MSG(false, "Failed to present swap chain image");
    }

    m_currentFrame = (m_currentFrame + 1) % VulkanContext::MAX_FRAMES_IN_FLIGHT;
    return true;
}

void VulkanRenderPass::waitForFences() {
    vkWaitForFences(m_context.getDevice(), 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);
}

void VulkanRenderPass::resetFences() {
    vkResetFences(m_context.getDevice(), 1, &m_inFlightFences[m_currentFrame]);
}

void VulkanRenderPass::createImage(u32 width, u32 height, u32 mipLevels, VkSampleCountFlagBits numSamples,
                                   VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
                                   VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory) {
    VkDevice device = m_context.getDevice();

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = mipLevels;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = numSamples;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkResult result = vkCreateImage(device, &imageInfo, nullptr, &image);
    CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to create image");

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = VulkanBuffer::findMemoryType(memRequirements.memoryTypeBits, properties, m_context.getPhysicalDevice());

    result = vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory);
    CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to allocate image memory");

    vkBindImageMemory(device, image, imageMemory, 0);
}

VkImageView VulkanRenderPass::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, u32 mipLevels) {
    VkDevice device = m_context.getDevice();

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;
    VkResult result = vkCreateImageView(device, &viewInfo, nullptr, &imageView);
    CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to create image view");

    return imageView;
}

VkSampleCountFlagBits VulkanRenderPass::getMaxUsableSampleCount() const {
    VkPhysicalDeviceProperties physicalDeviceProperties;
    vkGetPhysicalDeviceProperties(m_context.getPhysicalDevice(), &physicalDeviceProperties);

    VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts &
                                physicalDeviceProperties.limits.framebufferDepthSampleCounts;

    if (counts & VK_SAMPLE_COUNT_64_BIT) return VK_SAMPLE_COUNT_64_BIT;
    if (counts & VK_SAMPLE_COUNT_32_BIT) return VK_SAMPLE_COUNT_32_BIT;
    if (counts & VK_SAMPLE_COUNT_16_BIT) return VK_SAMPLE_COUNT_16_BIT;
    if (counts & VK_SAMPLE_COUNT_8_BIT) return VK_SAMPLE_COUNT_8_BIT;
    if (counts & VK_SAMPLE_COUNT_4_BIT) return VK_SAMPLE_COUNT_4_BIT;
    if (counts & VK_SAMPLE_COUNT_2_BIT) return VK_SAMPLE_COUNT_2_BIT;
    return VK_SAMPLE_COUNT_1_BIT;
}

}
