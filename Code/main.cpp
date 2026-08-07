#include <core/Log.hpp>
#include <graphics/Window.hpp>
#include <graphics/VulkanContext.hpp>
#include <graphics/VulkanPipeline.hpp>
#include <graphics/VulkanRenderPass.hpp>
#include <graphics/VulkanBuffer.hpp>
#include <graphics/VulkanDescriptor.hpp>
#include <graphics/Vertex.hpp>
#include <graphics/GlTFLoader.hpp>

#include <cstring>
#include <cmath>
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

int main() {
    crf::Log::init("engine.log");
    crf::Log::info("Engine v0.2.0 starting");

    crf::WindowConfig wc;
    wc.title = "Chronica Regna Fractorum";
    wc.width = 1920;
    wc.height = 1080;
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

    crf::Log::info("Creating Graphics Pipeline...");
    crf::VulkanPipeline pipeline(context, renderPass.getRenderPass(), renderPass.getMsaaSamples());
    pipeline.createDescriptorSetLayout();
    pipeline.createPipelineLayout(pipeline.getDescriptorSetLayout());
    pipeline.createGraphicsPipeline("shaders/cube.vert.spv", "shaders/cube.frag.spv");

    crf::Log::info("Creating mesh buffers...");
    const crf::MeshData& meshData = crf::loadScene("assets/models/test.glb");

    std::vector<crf::Vertex> vertices;
    vertices.reserve(meshData.positions.size() / 3);
    for (size_t i = 0; i < meshData.positions.size(); i += 3) {
        crf::Vertex vertex{};
        vertex.pos[0] = meshData.positions[i + 0];
        vertex.pos[1] = meshData.positions[i + 1];
        vertex.pos[2] = meshData.positions[i + 2];
        vertex.color[0] = 1.0f;
        vertex.color[1] = 1.0f;
        vertex.color[2] = 1.0f;
        vertex.texCoord[0] = 0.0f;
        vertex.texCoord[1] = 0.0f;
        vertices.push_back(vertex);
    }

    crf::VulkanBuffer buffers(context, renderPass.getCommandPool());
    buffers.createVertexBuffer(vertices);
    buffers.createIndexBuffer(meshData.indices);
    buffers.createUniformBuffers(1);

    crf::UniformBufferObject ubo{};
    glm::mat4 model = glm::mat4(1.0f);
    float aspect = static_cast<float>(context.getSwapChainExtent().width) / context.getSwapChainExtent().height;
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
    proj[1][1] *= -1.0f;

    std::memcpy(ubo.model, glm::value_ptr(model), sizeof(glm::mat4));
    std::memcpy(ubo.proj, glm::value_ptr(proj), sizeof(glm::mat4));

    float yaw = 45.0f;
    float pitch = 35.0f;
    float distance = 3.5f;

    crf::VulkanDescriptor descriptor(context, pipeline.getDescriptorSetLayout(), VK_NULL_HANDLE);
    descriptor.createDescriptorPool(1);
    descriptor.createDescriptorSets(buffers.getUniformBuffers(), 1);

    crf::Log::info("Entering main loop... (Press ESC to exit)");

    while (!window.shouldClose()) {
        window.pollEvents();

        if (window.isKeyJustPressed(GLFW_KEY_ESCAPE)) {
            break;
        }

        if (window.wasResized()) {
            renderPass.setFramebufferResized(true);
            window.clearResized();
        }

        if (window.isMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT)) {
            yaw += window.getMouseDeltaX() * 0.01f;
            pitch += window.getMouseDeltaY() * 0.01f;
            pitch = std::clamp(pitch, -89.0f, 89.0f);
        }

        distance *= std::pow(0.95f, window.getScrollDelta());
        distance = std::clamp(distance, 0.5f, 50.0f);

        float yawRad = glm::radians(yaw);
        float pitchRad = glm::radians(pitch);
        glm::vec3 eye(
            distance * std::cos(pitchRad) * std::sin(yawRad),
            distance * std::sin(pitchRad),
            distance * std::cos(pitchRad) * std::cos(yawRad));
        glm::mat4 view = glm::lookAt(eye, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

        std::memcpy(ubo.view, glm::value_ptr(view), sizeof(glm::mat4));
        buffers.updateUniformBuffer(0, ubo);

        renderPass.drawFrame([&](VkCommandBuffer commandBuffer, crf::u32) {
            VkExtent2D extent = context.getSwapChainExtent();

            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = static_cast<float>(extent.width);
            viewport.height = static_cast<float>(extent.height);
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

            VkRect2D scissor{};
            scissor.offset = {0, 0};
            scissor.extent = extent;
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.getGraphicsPipeline());

            VkDescriptorSet descriptorSet = descriptor.getDescriptorSets()[0];
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.getPipelineLayout(),
                                    0, 1, &descriptorSet, 0, nullptr);

            VkBuffer vertexBuffers[] = {buffers.getVertexBuffer()};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

            vkCmdBindIndexBuffer(commandBuffer, buffers.getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

            vkCmdDrawIndexed(commandBuffer, buffers.getIndexCount(), 1, 0, 0, 0);
        });
    }

    crf::Log::info("Main loop ended, cleaning up...");
    crf::Log::info("Engine shutdown");
    crf::Log::shutdown();
    return 0;
}
