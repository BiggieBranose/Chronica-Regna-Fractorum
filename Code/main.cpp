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
#include <array>
#include <iostream>

#include "game/header/Game.hpp"

struct RayTraceUBO {
    float viewInverse[16];
    float projInverse[16];
    float clearColor[4];
    uint32_t maxRecursionDepth;
    float padding[3];
};

struct Camera {
    float angle = 0.0f;
    float radius = 5.0f;
    float height = 3.0f;
    float targetX = 0.0f;
    float targetY = 0.0f;
    float targetZ = 0.0f;

    void update(float dt, float charX, float charZ) {
        targetX = charX;
        targetZ = charZ;
    }

    glm::mat4 getView() const {
        glm::vec3 eye(targetX + radius * sinf(angle), targetY + height, targetZ + radius * cosf(angle));
        return glm::lookAt(eye, glm::vec3(targetX, targetY + 0.3f, targetZ), glm::vec3(0.0f, 1.0f, 0.0f));
    }

    glm::mat4 getProj(float aspect) const {
        return glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
    }
};

void buildBoxGeometry(std::vector<crf::Vertex>& vertices, std::vector<uint32_t>& indices) {
    vertices = {
        // Front face (+Z) - Red
        {{-0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
        {{ 0.5f,  0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
        {{-0.5f,  0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
        // Back face (-Z) - Yellow
        {{ 0.5f, -0.5f, -0.5f}, {1.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
        {{-0.5f, -0.5f, -0.5f}, {1.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
        {{-0.5f,  0.5f, -0.5f}, {1.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},
        {{ 0.5f,  0.5f, -0.5f}, {1.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
        // Left face (-X) - Green
        {{-0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
        {{-0.5f, -0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
        {{-0.5f,  0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
        {{-0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},
        // Right face (+X) - Cyan
        {{ 0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},
        {{ 0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},
        {{ 0.5f,  0.5f,  0.5f}, {0.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
        {{ 0.5f, -0.5f,  0.5f}, {0.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},
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
        20, 21, 22, 22, 23, 20  // Bottom (-Y)
    };
}

static float terrainHeight(float x, float z) {
    float base = -0.5f;
    float h = 0.0f;

    auto smoothstep = [](float edge0, float edge1, float x) -> float {
        float t = (x - edge0) / (edge1 - edge0);
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        return t * t * (3.0f - 2.0f * t);
    };

    if (x > 1.5f && x < 6.5f && z > -2.5f && z < 2.5f) {
        float blend = smoothstep(1.5f, 2.5f, x) * smoothstep(1.5f, 2.5f, z)
                    * smoothstep(1.5f, 2.5f, 6.5f - x) * smoothstep(1.5f, 2.5f, 2.5f - z);
        h += 0.5f * blend;
    }

    if (x > -6.0f && x < -1.0f && z > 1.0f && z < 5.5f) {
        float blend = smoothstep(1.0f, 2.0f, x - (-6.0f)) * smoothstep(1.0f, 2.0f, z - 1.0f)
                    * smoothstep(1.0f, 2.0f, -1.0f - x) * smoothstep(1.0f, 2.0f, 5.5f - z);
        h += 1.0f * blend;
    }

    if (x > -3.5f && x < 1.5f && z > -6.0f && z < -2.0f) {
        float blend = smoothstep(1.0f, 2.0f, x - (-3.5f)) * smoothstep(1.0f, 2.0f, z - (-6.0f))
                    * smoothstep(1.0f, 2.0f, 1.5f - x) * smoothstep(1.0f, 2.0f, -2.0f - z);
        h += 0.3f * blend;
    }

    if (z > 3.0f) {
        float blend = smoothstep(3.0f, 4.0f, z);
        h += 0.25f * blend;
    }

    if (x < -4.5f && z < -0.5f) {
        float blend = smoothstep(-0.5f, -1.5f, z) * smoothstep(-4.5f, -5.5f, x);
        h += 0.7f * blend;
    }

    return base + h;
}

void buildGroundGeometry(std::vector<crf::Vertex>& vertices, std::vector<uint32_t>& indices, uint32_t baseVertex) {
    const int gridSize = 30;
    const float extent = 8.0f;
    float cellSize = (extent * 2.0f) / gridSize;
    float halfExtent = extent;

    vertices.clear();
    indices.clear();

    for (int z = 0; z < gridSize; z++) {
        for (int x = 0; x < gridSize; x++) {
            float x0 = -halfExtent + x * cellSize;
            float z0 = -halfExtent + z * cellSize;
            float x1 = x0 + cellSize;
            float z1 = z0 + cellSize;

            float h00 = terrainHeight(x0, z0);
            float h10 = terrainHeight(x1, z0);
            float h11 = terrainHeight(x1, z1);
            float h01 = terrainHeight(x0, z1);

            auto terrainColor = [](float h) -> std::array<float, 3> {
                float dh = h - (-0.5f);
                if (dh < 0.05f)  return {{0.30f, 0.52f, 0.22f}};
                if (dh < 0.15f)  return {{0.28f, 0.48f, 0.20f}};
                if (dh < 0.35f)  return {{0.45f, 0.38f, 0.25f}};
                if (dh < 0.6f)   return {{0.50f, 0.44f, 0.30f}};
                if (dh < 0.8f)   return {{0.42f, 0.38f, 0.32f}};
                return {{0.52f, 0.48f, 0.42f}};
            };

            uint32_t base = baseVertex + static_cast<uint32_t>(vertices.size());

            auto c = terrainColor(h00);
            vertices.push_back({{x0, h00, z0}, {c[0], c[1], c[2]}, {0.0f, 0.0f}});
            c = terrainColor(h10);
            vertices.push_back({{x1, h10, z0}, {c[0], c[1], c[2]}, {1.0f, 0.0f}});
            c = terrainColor(h11);
            vertices.push_back({{x1, h11, z1}, {c[0], c[1], c[2]}, {1.0f, 1.0f}});
            c = terrainColor(h01);
            vertices.push_back({{x0, h01, z1}, {c[0], c[1], c[2]}, {0.0f, 1.0f}});

            indices.push_back(base + 0);
            indices.push_back(base + 2);
            indices.push_back(base + 1);
            indices.push_back(base + 2);
            indices.push_back(base + 0);
            indices.push_back(base + 3);
        }
    }
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
    rasterPipeline.createRayQueryDescriptorSetLayout();
    rasterPipeline.createPipelineLayout(rasterPipeline.getDescriptorSetLayout());
    crf::Log::info("Creating graphics pipeline (loading shaders)...");
    rasterPipeline.createGraphicsPipeline("shaders/box.vert.spv", "shaders/box.frag.spv");

    crf::Log::info("Creating ground geometry...");
    std::vector<crf::Vertex> vertices;
    std::vector<uint32_t> indices;
    buildGroundGeometry(vertices, indices, 0);

    crf::VulkanBuffer buffer(context, renderPass.getCommandPool());
    buffer.createVertexBuffer(vertices);
    buffer.createIndexBuffer(indices);
    buffer.createUniformBuffers(crf::VulkanContext::MAX_FRAMES_IN_FLIGHT);

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
    }

    crf::Log::info("Creating descriptors...");
    crf::VulkanDescriptor descriptor(context, rasterPipeline.getDescriptorSetLayout(), nullptr);
    if (hasRaytracing && accelStruct) {
        descriptor.createRayQueryDescriptorPool(crf::VulkanContext::MAX_FRAMES_IN_FLIGHT);
        descriptor.createRayQueryDescriptorSets(buffer.getUniformBuffers(), crf::VulkanContext::MAX_FRAMES_IN_FLIGHT, accelStruct->getTopLevelAS());
    } else {
        descriptor.createDescriptorPool(crf::VulkanContext::MAX_FRAMES_IN_FLIGHT);
        descriptor.createDescriptorSets(buffer.getUniformBuffers(), crf::VulkanContext::MAX_FRAMES_IN_FLIGHT, VK_NULL_HANDLE, VK_NULL_HANDLE);
    }

    Camera camera;
    bool useRaytracing = hasRaytracing;

    VkDevice device = context.getDevice();

    if (hasRaytracing && accelStruct) {
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
                                                    accelStruct->getVertexBuffer(), 
                                                    sizeof(crf::Vertex) * vertices.size(),
                                                    rtCameraBuffer, camBufferSize);

        crf::Log::info("Raytracing infrastructure ready");
    }

    game::Game game(context, renderPass, window);
    game.init();

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

        camera.update(dt, game.getCharX(), game.getCharZ());
        camera.targetY = game.getCharY();
        game.update(dt);

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
            VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR};
            VkSemaphore signalSemaphore = renderPass.getPerImageSemaphore(imageIndex);

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
                renderPass.cleanupSwapChain();
                renderPass.createColorResources();
                renderPass.createDepthResources();
                renderPass.createFramebuffers();
            } else if (result != VK_SUCCESS) {
                CRF_ASSERT_MSG(false, "Failed to present swap chain image");
            }

            // Advance frame
            renderPass.advanceFrame();
        } else {
            // Rasterization path
            crf::UniformBufferObject ubo{};
            glm::mat4 model = glm::mat4(1.0f);
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

                game.render(cmd, imageIndex, glm::value_ptr(view), glm::value_ptr(proj));
            });
        }
    }

    vkDeviceWaitIdle(device);

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