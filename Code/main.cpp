#include <core/Log.hpp>
#include <graphics/Window.hpp>
#include <graphics/VulkanContext.hpp>
#include <graphics/VulkanRenderPass.hpp>
#include <graphics/VulkanPipeline.hpp>
#include <graphics/VulkanBuffer.hpp>
#include <graphics/VulkanTexture.hpp>
#include <graphics/VulkanDescriptor.hpp>
#include <graphics/ModelLoader.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <chrono>
#include <cstring>
#include <vector>
#include <cmath>

int main() {
    crf::Log::init("engine.log");
    crf::Log::info("Engine v0.2.0 starting");

    crf::WindowConfig wc;
    wc.title = "Chronica Regna Fractorum";
    wc.width = 1280;
    wc.height = 720;
    wc.vsync = true;

    crf::Window window(wc);

    crf::Log::info("Creating VulkanContext...");
    crf::VulkanContext context(window);
    crf::VulkanRenderPass renderPass(context);

    crf::Log::info("Creating render pass...");
    renderPass.createRenderPass();
    crf::Log::info("Creating color resources...");
    renderPass.createColorResources();
    crf::Log::info("Creating depth resources...");
    renderPass.createDepthResources();
    crf::Log::info("Creating framebuffers...");
    renderPass.createFramebuffers();
    crf::Log::info("Creating command pool...");
    renderPass.createCommandPool();
    crf::Log::info("Creating command buffers...");
    renderPass.createCommandBuffers();
    crf::Log::info("Creating sync objects...");
    renderPass.createSyncObjects();

    crf::Log::info("Creating pipeline...");
    crf::VulkanPipeline pipeline(context, renderPass.getRenderPass(), renderPass.getMsaaSamples());
    pipeline.createDescriptorSetLayout();
    pipeline.createPipelineLayout(pipeline.getDescriptorSetLayout());
    crf::Log::info("Creating graphics pipeline (loading shaders)...");
    pipeline.createGraphicsPipeline();

    crf::Log::info("Creating buffers...");
    crf::VulkanBuffer buffer(context, renderPass.getCommandPool());

    std::vector<crf::Vertex> vertices = {
        {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
        {{ 0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
        {{-0.5f,  0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}
    };

    std::vector<uint32_t> indices = {0, 1, 2, 2, 3, 0};

    buffer.createVertexBuffer(vertices);
    buffer.createIndexBuffer(indices);
    buffer.createUniformBuffers(crf::VulkanContext::MAX_FRAMES_IN_FLIGHT);

    crf::Log::info("Creating dummy texture...");
    VkDevice device = context.getDevice();
    uint8_t whitePixel[4] = {255, 255, 255, 255};
    VkDeviceSize imageSize = 4;

    VkBuffer stagingBuf;
    VkDeviceMemory stagingMem;
    VkBufferCreateInfo bufInfo{};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size = imageSize;
    bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vkCreateBuffer(device, &bufInfo, nullptr, &stagingBuf);

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device, stagingBuf, &memReqs);
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = crf::VulkanBuffer::findMemoryType(
        memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        context.getPhysicalDevice()
    );
    vkAllocateMemory(device, &allocInfo, nullptr, &stagingMem);
    vkBindBufferMemory(device, stagingBuf, stagingMem, 0);
    void* data;
    vkMapMemory(device, stagingMem, 0, imageSize, 0, &data);
    std::memcpy(data, whitePixel, imageSize);
    vkUnmapMemory(device, stagingMem);

    VkImage dummyImage;
    VkDeviceMemory dummyImageMemory;
    VkImageCreateInfo imgInfo{};
    imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.extent.width = 1;
    imgInfo.extent.height = 1;
    imgInfo.extent.depth = 1;
    imgInfo.mipLevels = 1;
    imgInfo.arrayLayers = 1;
    imgInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imgInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vkCreateImage(device, &imgInfo, nullptr, &dummyImage);

    VkMemoryRequirements imgMemReqs;
    vkGetImageMemoryRequirements(device, dummyImage, &imgMemReqs);
    VkMemoryAllocateInfo imgAllocInfo{};
    imgAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    imgAllocInfo.allocationSize = imgMemReqs.size;
    imgAllocInfo.memoryTypeIndex = crf::VulkanBuffer::findMemoryType(
        imgMemReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, context.getPhysicalDevice()
    );
    vkAllocateMemory(device, &imgAllocInfo, nullptr, &dummyImageMemory);
    vkBindImageMemory(device, dummyImage, dummyImageMemory, 0);

    VkCommandBuffer cmd = renderPass.beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = dummyImage;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkBufferImageCopy copyRegion{};
    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageSubresource.layerCount = 1;
    copyRegion.imageExtent = {1, 1, 1};
    vkCmdCopyBufferToImage(cmd, stagingBuf, dummyImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);

    renderPass.endSingleTimeCommands(cmd);

    vkDestroyBuffer(device, stagingBuf, nullptr);
    vkFreeMemory(device, stagingMem, nullptr);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = dummyImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;
    VkImageView dummyImageView;
    vkCreateImageView(device, &viewInfo, nullptr, &dummyImageView);

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.minLod = 0;
    samplerInfo.maxLod = 0;
    samplerInfo.mipLodBias = 0;
    VkSampler dummySampler;
    vkCreateSampler(device, &samplerInfo, nullptr, &dummySampler);

    crf::Log::info("Creating descriptors...");
    crf::VulkanDescriptor descriptor(context, pipeline.getDescriptorSetLayout(), nullptr);
    descriptor.createDescriptorPool(crf::VulkanContext::MAX_FRAMES_IN_FLIGHT);
    descriptor.createDescriptorSets(buffer.getUniformBuffers(), crf::VulkanContext::MAX_FRAMES_IN_FLIGHT, dummyImageView, dummySampler);

    crf::Log::info("Entering main loop...");

    auto startTime = std::chrono::high_resolution_clock::now();

    while (!window.shouldClose()) {
        window.pollEvents();

        if (window.isKeyJustPressed(GLFW_KEY_ESCAPE)) {
            break;
        }

        if (window.wasResized()) {
            renderPass.setFramebufferResized(true);
            window.clearResized();
        }

        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float>(currentTime - startTime).count();

        crf::UniformBufferObject ubo{};

        float s = std::cos(time);
        float c = std::sin(time);

        float model[16] = {
             s, -c, 0.0f, 0.0f,
             c,  s, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        };
        std::memcpy(ubo.model, model, sizeof(model));

        float view[16] = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, -2.0f, 1.0f
        };
        std::memcpy(ubo.view, view, sizeof(view));

        float proj[16] = {};
        float aspect = static_cast<float>(window.getWidth()) / static_cast<float>(window.getHeight());
        float fov = 45.0f;
        float nearPlane = 0.1f;
        float farPlane = 10.0f;

        float tanHalfFov = std::tan(fov * 3.14159265f / 360.0f);

        proj[0] = 1.0f / (aspect * tanHalfFov);
        proj[5] = -1.0f / tanHalfFov;
        proj[10] = farPlane / (nearPlane - farPlane);
        proj[11] = -1.0f;
        proj[14] = (nearPlane * farPlane) / (nearPlane - farPlane);

        std::memcpy(ubo.proj, proj, sizeof(proj));

        buffer.updateUniformBuffer(renderPass.getCurrentFrame(), ubo);

        renderPass.drawFrame([&](VkCommandBuffer cmd) {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.getGraphicsPipeline());

            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = static_cast<float>(context.getSwapChainExtent().width);
            viewport.height = static_cast<float>(context.getSwapChainExtent().height);
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(cmd, 0, 1, &viewport);

            VkRect2D scissor{};
            scissor.offset = {0, 0};
            scissor.extent = context.getSwapChainExtent();
            vkCmdSetScissor(cmd, 0, 1, &scissor);

            VkBuffer vertexBuffers[] = {buffer.getVertexBuffer()};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
            vkCmdBindIndexBuffer(cmd, buffer.getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    pipeline.getPipelineLayout(), 0, 1,
                                    &descriptor.getDescriptorSets()[renderPass.getCurrentFrame()],
                                    0, nullptr);

            vkCmdDrawIndexed(cmd, buffer.getIndexCount(), 1, 0, 0, 0);
        });
    }

    vkDeviceWaitIdle(context.getDevice());

    vkDestroySampler(device, dummySampler, nullptr);
    vkDestroyImageView(device, dummyImageView, nullptr);
    vkDestroyImage(device, dummyImage, nullptr);
    vkFreeMemory(device, dummyImageMemory, nullptr);

    crf::Log::info("Main loop ended, cleaning up...");
    crf::Log::info("Engine shutdown");
    crf::Log::shutdown();
    return 0;
}
