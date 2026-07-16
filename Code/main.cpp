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

    crf::VulkanContext context(window);
    crf::VulkanRenderPass renderPass(context);

    renderPass.createRenderPass();
    renderPass.createColorResources();
    renderPass.createDepthResources();
    renderPass.createFramebuffers();
    renderPass.createCommandPool();
    renderPass.createCommandBuffers();
    renderPass.createSyncObjects();

    crf::VulkanPipeline pipeline(context, renderPass.getRenderPass(), renderPass.getMsaaSamples());
    pipeline.createDescriptorSetLayout();
    pipeline.createPipelineLayout(pipeline.getDescriptorSetLayout());
    pipeline.createGraphicsPipeline();

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

    crf::VulkanDescriptor descriptor(context, pipeline.getDescriptorSetLayout(), nullptr);
    descriptor.createDescriptorPool(crf::VulkanContext::MAX_FRAMES_IN_FLIGHT);
    descriptor.createDescriptorSets(buffer.getUniformBuffers(), crf::VulkanContext::MAX_FRAMES_IN_FLIGHT);

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
            0.0f, 0.0f, 2.0f, 1.0f
        };
        std::memcpy(ubo.view, view, sizeof(view));

        float proj[16] = {};
        float aspect = static_cast<float>(window.getWidth()) / static_cast<float>(window.getHeight());
        float fov = 45.0f;
        float nearPlane = 0.1f;
        float farPlane = 10.0f;

        float tanHalfFov = std::tan(fov * 3.14159265f / 360.0f);

        proj[0] = 1.0f / (aspect * tanHalfFov);
        proj[5] = 1.0f / tanHalfFov;
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

    crf::Log::info("Engine shutdown");
    crf::Log::shutdown();
    return 0;
}
