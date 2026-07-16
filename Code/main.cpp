#include <core/Log.hpp>
#include <core/Assert.hpp>
#include <graphics/Window.hpp>
#include <graphics/VulkanContext.hpp>
#include <graphics/VulkanRenderPass.hpp>
#include <graphics/VulkanPipeline.hpp>
#include <graphics/VulkanBuffer.hpp>
#include <graphics/VulkanTexture.hpp>
#include <graphics/VulkanDescriptor.hpp>
#include <graphics/ModelLoader.hpp>
#include <graphics/AccelerationStructure.hpp>
#include <graphics/RaytracingPipeline.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <chrono>
#include <cstring>
#include <vector>
#include <cmath>
#include <iostream>

struct RayTraceUBO {
    float viewInverse[16];
    float projInverse[16];
    float clearColor[4];
    uint32_t maxRecursionDepth;
    float padding[3];
};

struct Camera {
    float angle = 0.0f;
    float radius = 3.0f;
    float height = 1.5f;

    void update(float dt) {
        angle += dt * 0.5f;
    }

    glm::mat4 getView() const {
        glm::vec3 eye(radius * sin(angle), height, radius * cos(angle));
        return glm::lookAt(eye, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    }

    glm::mat4 getProj(float aspect) const {
        return glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
    }
};

void buildBoxGeometry(std::vector<crf::Vertex>& vertices, std::vector<uint32_t>& indices) {
    // For raytracing closest-hit shader, need 3 vec4 per vertex (pos, color, uv as vec4)
    // CPU Vertex is 32 bytes (3+3+2 floats), RT shader expects 48 bytes (3 vec4)
    // Create a separate RT vertex buffer with packed vec4 format
    vertices = {
        // Front face (+Z) - Red
        {{-0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
        {{ 0.5f,  0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
        {{-0.5f,  0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
        // Back face (-Z) - Yellow
        {{ 0.5f, -0.5f, -0.5f}, {1.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
        {{-0.5f, -0.5f, -0.5f}, {1.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
        {{-0.5f,  0.5f, -0.5f}, {1.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
        {{ 0.5f,  0.5f, -0.5f}, {1.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},
        // Left face (-X) - Green
        {{-0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
        {{-0.5f, -0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
        {{-0.5f,  0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
        {{-0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},
        // Right face (+X) - Cyan
        {{ 0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},
        {{ 0.5f, -0.5f,  0.5f}, {0.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},
        {{ 0.5f,  0.5f,  0.5f}, {0.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
        {{ 0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},
        // Top face (+Y) - Blue
        {{-0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
        {{ 0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
        {{ 0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
        {{-0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
        // Bottom face (-Y) - Magenta
        {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
        {{ 0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
        {{ 0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
        {{-0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
    };

    indices = {
        0, 1, 2, 2, 3, 0,      // Front (+Z)
        4, 5, 6, 6, 7, 4,      // Back (-Z)
        8, 9, 10, 10, 11, 8,   // Left (-X)
        12, 13, 14, 14, 15, 12, // Right (+X)
        16, 17, 18, 18, 19, 16, // Top (+Y)
        20, 21, 22, 22, 23, 20  // Bottom (-Y) - fixed CCW winding for -Y normal
    };
}

int main() {
    crf::Log::init("engine.log");
    crf::Log::info("Engine v0.2.0 starting");

    crf::WindowConfig wc;
    wc.title = "Chronica Regna Fractorum - Raytracing Toggle Demo";
    wc.width = 1280;
    wc.height = 720;
    wc.vsync = true;

    crf::Window window(wc);

    crf::Log::info("Creating VulkanContext...");
    crf::VulkanContext context(window);

    bool hasRaytracing = context.hasRaytracing();
    crf::Log::info("Raytracing support: {}", hasRaytracing ? "YES" : "NO");

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

    crf::Log::info("Creating raster pipeline...");
    crf::VulkanPipeline rasterPipeline(context, renderPass.getRenderPass(), renderPass.getMsaaSamples());
    rasterPipeline.createDescriptorSetLayout();
    rasterPipeline.createPipelineLayout(rasterPipeline.getDescriptorSetLayout());
    crf::Log::info("Creating graphics pipeline (loading shaders)...");
    rasterPipeline.createGraphicsPipeline("shaders/box.vert.spv", "shaders/box.frag.spv");

    crf::Log::info("Creating box geometry...");
    std::vector<crf::Vertex> vertices;
    std::vector<uint32_t> indices;
    buildBoxGeometry(vertices, indices);

    crf::VulkanBuffer buffer(context, renderPass.getCommandPool());
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
    crf::VulkanDescriptor descriptor(context, rasterPipeline.getDescriptorSetLayout(), nullptr);
    descriptor.createDescriptorPool(crf::VulkanContext::MAX_FRAMES_IN_FLIGHT);
    descriptor.createDescriptorSets(buffer.getUniformBuffers(), crf::VulkanContext::MAX_FRAMES_IN_FLIGHT, dummyImageView, dummySampler);

    Camera camera;
    bool useRaytracing = hasRaytracing;

    crf::AccelerationStructure* accelStruct = nullptr;
    crf::RaytracingPipeline* rtPipeline = nullptr;
    VkImage rtOutputImage = nullptr;
    VkDeviceMemory rtOutputMemory = nullptr;
    VkImageView rtOutputView = nullptr;
    VkBuffer rtCameraBuffer = nullptr;
    VkDeviceMemory rtCameraMemory = nullptr;

    if (hasRaytracing) {
        crf::Log::info("Creating raytracing infrastructure...");
        accelStruct = new crf::AccelerationStructure(context, renderPass.getCommandPool());
        accelStruct->buildBottomLevelAccelerationStructure(vertices, indices);
        accelStruct->buildTopLevelAccelerationStructure(1);

        rtPipeline = new crf::RaytracingPipeline(context, *accelStruct);
        rtPipeline->createRaytracingDescriptorSetLayout();
        rtPipeline->createRaytracingPipeline();
        rtPipeline->createRaytracingDescriptorPool();

        VkExtent2D extent = context.getSwapChainExtent();
        VkImageCreateInfo rtImgInfo{};
        rtImgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        rtImgInfo.imageType = VK_IMAGE_TYPE_2D;
        rtImgInfo.extent.width = extent.width;
        rtImgInfo.extent.height = extent.height;
        rtImgInfo.extent.depth = 1;
        rtImgInfo.mipLevels = 1;
        rtImgInfo.arrayLayers = 1;
        rtImgInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        rtImgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        rtImgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        rtImgInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        rtImgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        rtImgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vkCreateImage(device, &rtImgInfo, nullptr, &rtOutputImage);

        VkMemoryRequirements rtImgMemReqs;
        vkGetImageMemoryRequirements(device, rtOutputImage, &rtImgMemReqs);
        VkMemoryAllocateInfo rtImgAllocInfo{};
        rtImgAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        rtImgAllocInfo.allocationSize = rtImgMemReqs.size;
        rtImgAllocInfo.memoryTypeIndex = crf::VulkanBuffer::findMemoryType(
            rtImgMemReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, context.getPhysicalDevice()
        );
        vkAllocateMemory(device, &rtImgAllocInfo, nullptr, &rtOutputMemory);
        vkBindImageMemory(device, rtOutputImage, rtOutputMemory, 0);

        VkImageViewCreateInfo rtViewInfo{};
        rtViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        rtViewInfo.image = rtOutputImage;
        rtViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        rtViewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        rtViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        rtViewInfo.subresourceRange.levelCount = 1;
        rtViewInfo.subresourceRange.layerCount = 1;
        vkCreateImageView(device, &rtViewInfo, nullptr, &rtOutputView);

        VkDeviceSize camBufferSize = sizeof(RayTraceUBO);
        VkBufferCreateInfo camBufInfo{};
        camBufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        camBufInfo.size = camBufferSize;
        camBufInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        camBufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vkCreateBuffer(device, &camBufInfo, nullptr, &rtCameraBuffer);

        VkMemoryRequirements camMemReqs;
        vkGetBufferMemoryRequirements(device, rtCameraBuffer, &camMemReqs);
        VkMemoryAllocateInfo camAllocInfo{};
        camAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        camAllocInfo.allocationSize = camMemReqs.size;
        camAllocInfo.memoryTypeIndex = crf::VulkanBuffer::findMemoryType(
            camMemReqs.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            context.getPhysicalDevice()
        );
        vkAllocateMemory(device, &camAllocInfo, nullptr, &rtCameraMemory);
        vkBindBufferMemory(device, rtCameraBuffer, rtCameraMemory, 0);

        rtPipeline->createRaytracingDescriptorSets(rtOutputView, VK_NULL_HANDLE,
                                                    accelStruct->getRtVertexBuffer(), 
                                                    sizeof(float) * 12 * vertices.size(),
                                                    rtCameraBuffer, camBufferSize);

        crf::Log::info("Raytracing infrastructure ready");
    }

    crf::Log::info("Entering main loop... (Press R to toggle raytracing)");

    auto startTime = std::chrono::high_resolution_clock::now();
    float lastTime = 0.0f;

    while (!window.shouldClose()) {
        window.pollEvents();

        if (window.isKeyJustPressed(GLFW_KEY_ESCAPE)) {
            break;
        }

        if (window.isKeyJustPressed(GLFW_KEY_R)) {
            if (hasRaytracing) {
                useRaytracing = !useRaytracing;
                crf::Log::info("Switched to {} rendering", useRaytracing ? "RAYTRACING" : "RASTERIZATION");
            } else {
                crf::Log::info("Raytracing not supported on this hardware");
            }
        }

        if (window.wasResized()) {
            renderPass.setFramebufferResized(true);
            window.clearResized();
        }

        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float>(currentTime - startTime).count();
        float dt = time - lastTime;
        lastTime = time;

        camera.update(dt);

        float aspect = static_cast<float>(window.getWidth()) / static_cast<float>(window.getHeight());
        glm::mat4 view = camera.getView();
        glm::mat4 proj = camera.getProj(aspect);
        proj[1][1] *= -1;

        if (useRaytracing && hasRaytracing) {
            // Raytracing path - proper synchronization
            crf::u32 currentFrame = renderPass.getCurrentFrame();
            VkDevice device = context.getDevice();

            VkFence inFlightFence = renderPass.getInFlightFence(currentFrame);
            vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);

            // Acquire next image
            uint32_t imageIndex;
            VkResult result = vkAcquireNextImageKHR(device, context.getSwapChain(), UINT64_MAX,
                                                     renderPass.getImageAvailableSemaphore(currentFrame),
                                                     VK_NULL_HANDLE, &imageIndex);

            if (result == VK_ERROR_OUT_OF_DATE_KHR) {
                renderPass.setFramebufferResized(true);
                continue;
            }
            if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
                CRF_ASSERT_MSG(false, "Failed to acquire swap chain image");
            }

            VkFence imageInFlightFence = renderPass.getImageInFlight(imageIndex);
            if (imageInFlightFence != VK_NULL_HANDLE) {
                vkWaitForFences(device, 1, &imageInFlightFence, VK_TRUE, UINT64_MAX);
            }

            // Mark image as in-flight
            renderPass.setImageInFlight(imageIndex, inFlightFence);

            // Reset the in-flight fence
            vkResetFences(device, 1, &inFlightFence);

            // Update RT camera UBO
            RayTraceUBO rtUBO{};
            glm::mat4 viewInv = glm::inverse(view);
            glm::mat4 projInv = glm::inverse(proj);
            std::memcpy(rtUBO.viewInverse, glm::value_ptr(viewInv), sizeof(float) * 16);
            std::memcpy(rtUBO.projInverse, glm::value_ptr(projInv), sizeof(float) * 16);
            rtUBO.clearColor[0] = 0.5f;
            rtUBO.clearColor[1] = 0.7f;
            rtUBO.clearColor[2] = 1.0f;
            rtUBO.clearColor[3] = 1.0f;
            rtUBO.maxRecursionDepth = 1;

            void* mapped;
            vkMapMemory(device, rtCameraMemory, 0, sizeof(RayTraceUBO), 0, &mapped);
            std::memcpy(mapped, &rtUBO, sizeof(RayTraceUBO));
            vkUnmapMemory(device, rtCameraMemory);

            // Record raytracing commands
            VkCommandBuffer cmd = renderPass.getCommandBuffer(currentFrame);
            vkResetCommandBuffer(cmd, 0);

            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            vkBeginCommandBuffer(cmd, &beginInfo);

            // Transition RT output image to GENERAL
            VkImageMemoryBarrier rtBarrier{};
            rtBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            rtBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            rtBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            rtBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            rtBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            rtBarrier.image = rtOutputImage;
            rtBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            rtBarrier.subresourceRange.baseMipLevel = 0;
            rtBarrier.subresourceRange.levelCount = 1;
            rtBarrier.subresourceRange.baseArrayLayer = 0;
            rtBarrier.subresourceRange.layerCount = 1;
            rtBarrier.srcAccessMask = 0;
            rtBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                                 0, 0, nullptr, 0, nullptr, 1, &rtBarrier);

            // Trace rays
            rtPipeline->recordRaytracingCommands(cmd, context.getSwapChainExtent().width, context.getSwapChainExtent().height);

            // Transition RT output to TRANSFER_SRC
            VkImageMemoryBarrier rtBarrier2{};
            rtBarrier2.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            rtBarrier2.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
            rtBarrier2.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            rtBarrier2.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            rtBarrier2.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            rtBarrier2.image = rtOutputImage;
            rtBarrier2.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            rtBarrier2.subresourceRange.baseMipLevel = 0;
            rtBarrier2.subresourceRange.levelCount = 1;
            rtBarrier2.subresourceRange.baseArrayLayer = 0;
            rtBarrier2.subresourceRange.layerCount = 1;
            rtBarrier2.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            rtBarrier2.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 0, 0, nullptr, 0, nullptr, 1, &rtBarrier2);

            // Transition swapchain image to TRANSFER_DST
            VkImageMemoryBarrier swapBarrier{};
            swapBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            swapBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            swapBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            swapBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            swapBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            swapBarrier.image = context.getSwapChainImages()[imageIndex];
            swapBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            swapBarrier.subresourceRange.baseMipLevel = 0;
            swapBarrier.subresourceRange.levelCount = 1;
            swapBarrier.subresourceRange.baseArrayLayer = 0;
            swapBarrier.subresourceRange.layerCount = 1;
            swapBarrier.srcAccessMask = 0;
            swapBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 0, 0, nullptr, 0, nullptr, 1, &swapBarrier);

            // Blit RT output to swapchain
            VkImageBlit blit{};
            blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.srcSubresource.layerCount = 1;
            blit.srcOffsets[0] = {0, 0, 0};
            blit.srcOffsets[1] = {static_cast<int32_t>(context.getSwapChainExtent().width), static_cast<int32_t>(context.getSwapChainExtent().height), 1};
            blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.dstSubresource.layerCount = 1;
            blit.dstOffsets[0] = {0, 0, 0};
            blit.dstOffsets[1] = {static_cast<int32_t>(context.getSwapChainExtent().width), static_cast<int32_t>(context.getSwapChainExtent().height), 1};
            vkCmdBlitImage(cmd, rtOutputImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           context.getSwapChainImages()[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1, &blit, VK_FILTER_LINEAR);

            // Transition swapchain to PRESENT_SRC
            VkImageMemoryBarrier presentBarrier{};
            presentBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            presentBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            presentBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            presentBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            presentBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            presentBarrier.image = context.getSwapChainImages()[imageIndex];
            presentBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            presentBarrier.subresourceRange.baseMipLevel = 0;
            presentBarrier.subresourceRange.levelCount = 1;
            presentBarrier.subresourceRange.baseArrayLayer = 0;
            presentBarrier.subresourceRange.layerCount = 1;
            presentBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            presentBarrier.dstAccessMask = 0;
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                 0, 0, nullptr, 0, nullptr, 1, &presentBarrier);

            vkEndCommandBuffer(cmd);

            // Submit
            VkSubmitInfo submitInfo{};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            VkSemaphore waitSemaphores[] = {renderPass.getImageAvailableSemaphore(currentFrame)};
            VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
            VkSemaphore signalSemaphore = renderPass.getRenderFinishedSemaphore(currentFrame);

            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores = waitSemaphores;
            submitInfo.pWaitDstStageMask = waitStages;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &cmd;
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = &signalSemaphore;

            VkResult submitResult = vkQueueSubmit(context.getGraphicsQueue(), 1, &submitInfo, renderPass.getInFlightFence(currentFrame));
            CRF_ASSERT_MSG(submitResult == VK_SUCCESS, "Failed to submit raytracing command buffer");

            // Present
            VkPresentInfoKHR presentInfo{};
            presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores = &signalSemaphore;
            VkSwapchainKHR swapChains[] = {context.getSwapChain()};
            presentInfo.swapchainCount = 1;
            presentInfo.pSwapchains = swapChains;
            presentInfo.pImageIndices = &imageIndex;

            result = vkQueuePresentKHR(context.getPresentQueue(), &presentInfo);

            if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || renderPass.wasFramebufferResized()) {
                renderPass.setFramebufferResized(false);
                context.recreateSwapChain();
            } else if (result != VK_SUCCESS) {
                CRF_ASSERT_MSG(false, "Failed to present swap chain image");
            }

            // Advance frame
            renderPass.advanceFrame();
        } else {
            // Rasterization path
            crf::UniformBufferObject ubo{};
            glm::mat4 model = glm::rotate(glm::mat4(1.0f), time, glm::vec3(0.0f, 1.0f, 0.0f));
            std::memcpy(ubo.model, glm::value_ptr(model), sizeof(float) * 16);
            std::memcpy(ubo.view, glm::value_ptr(view), sizeof(float) * 16);
            std::memcpy(ubo.proj, glm::value_ptr(proj), sizeof(float) * 16);

            buffer.updateUniformBuffer(renderPass.getCurrentFrame(), ubo);

            renderPass.drawFrame([&](VkCommandBuffer cmd, crf::u32 imageIndex) {
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, rasterPipeline.getGraphicsPipeline());

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
                                        rasterPipeline.getPipelineLayout(), 0, 1,
                                        &descriptor.getDescriptorSets()[renderPass.getCurrentFrame()],
                                        0, nullptr);

                vkCmdDrawIndexed(cmd, buffer.getIndexCount(), 1, 0, 0, 0);
            });
        }
    }

    vkDeviceWaitIdle(device);

    vkDestroySampler(device, dummySampler, nullptr);
    vkDestroyImageView(device, dummyImageView, nullptr);
    vkDestroyImage(device, dummyImage, nullptr);
    vkFreeMemory(device, dummyImageMemory, nullptr);

    if (hasRaytracing) {
        vkDestroyImageView(device, rtOutputView, nullptr);
        vkDestroyImage(device, rtOutputImage, nullptr);
        vkFreeMemory(device, rtOutputMemory, nullptr);
        vkDestroyBuffer(device, rtCameraBuffer, nullptr);
        vkFreeMemory(device, rtCameraMemory, nullptr);
        delete rtPipeline;
        delete accelStruct;
    }

    crf::Log::info("Main loop ended, cleaning up...");
    crf::Log::info("Engine shutdown");
    crf::Log::shutdown();
    return 0;
}