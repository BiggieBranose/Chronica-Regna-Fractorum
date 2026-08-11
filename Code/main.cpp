#include <core/Log.hpp>
#include <core/Types.hpp>
#include <graphics/Window.hpp>
#include <graphics/VulkanContext.hpp>
#include <graphics/VulkanPipeline.hpp>
#include <graphics/VulkanRenderPass.hpp>
#include <graphics/VulkanBuffer.hpp>
#include <graphics/VulkanDescriptor.hpp>
#include <graphics/VulkanTexture.hpp>
#include <graphics/Vertex.hpp>
#include <graphics/GlTFLoader.hpp>
#include <graphics/Scene.hpp>

#include <cstring>
#include <cmath>
#include <algorithm>
#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

std::vector<crf::Vertex> makeSkyCube() {
    constexpr crf::f32 s = 100.0f;

    std::vector<crf::Vertex> verts;
    verts.reserve(36);

    auto addFace = [&](crf::f32 x, crf::f32 y, crf::f32 z, crf::f32 ux, crf::f32 uy, crf::f32 uz, crf::f32 vx, crf::f32 vy, crf::f32 vz) {
        auto addVert = [&](crf::f32 px, crf::f32 py, crf::f32 pz) {
            crf::Vertex v{};
            v.pos[0] = px;
            v.pos[1] = py;
            v.pos[2] = pz;
            verts.push_back(v);
        };
        addVert(x - ux - vx, y - uy - vy, z - uz - vz);
        addVert(x + ux - vx, y + uy - vy, z + uz - vz);
        addVert(x + ux + vx, y + uy + vy, z + uz + vz);
        addVert(x - ux - vx, y - uy - vy, z - uz - vz);
        addVert(x + ux + vx, y + uy + vy, z + uz + vz);
        addVert(x - ux + vx, y - uy + vy, z - uz + vz);
    };

    addFace(0, 0, s, s, 0, 0, 0, s, 0);
    addFace(0, 0, -s, -s, 0, 0, 0, s, 0);
    addFace(s, 0, 0, 0, 0, s, 0, s, 0);
    addFace(-s, 0, 0, 0, 0, -s, 0, s, 0);
    addFace(0, s, 0, s, 0, 0, 0, 0, s);
    addFace(0, -s, 0, s, 0, 0, 0, 0, -s);

    return verts;
}

struct MeshBuffers {
    crf::Scope<crf::VulkanBuffer> buffer;
};

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
    pipeline.createSkyPipeline();

    crf::Log::info("Creating sky cube...");
    crf::VulkanBuffer skyBuffers(context, renderPass.getCommandPool());
    skyBuffers.createVertexBuffer(makeSkyCube());

    crf::Log::info("Building scene...");
    crf::Scene scene;
    scene.loadSceneFile("assets/models/test_scene.glb");
    const crf::u32 playerMesh = scene.loadMeshFile("assets/models/test_cube.glb");

    crf::Transform playerTransform;
    playerTransform.position = glm::vec3(0.0f, 0.5f, 0.0f);
    playerTransform.scale = glm::vec3(0.4f);
    const crf::u32 playerEntity = scene.addEntity("Player", playerMesh, playerTransform.toMat4());

    std::vector<MeshBuffers> meshBuffers;
    for (const crf::MeshData& mesh : scene.getMeshes()) {
        std::vector<crf::Vertex> vertices;
        vertices.reserve(mesh.positions.size() / 3);
        for (size_t i = 0; i < mesh.positions.size(); i += 3) {
            crf::Vertex vertex{};
            vertex.pos[0] = mesh.positions[i + 0];
            vertex.pos[1] = mesh.positions[i + 1];
            vertex.pos[2] = mesh.positions[i + 2];
            vertex.texCoord[0] = mesh.texCoords[(i / 3) * 2 + 0];
            vertex.texCoord[1] = mesh.texCoords[(i / 3) * 2 + 1];
            vertex.normal[0] = mesh.normals[(i / 3) * 3 + 0];
            vertex.normal[1] = mesh.normals[(i / 3) * 3 + 1];
            vertex.normal[2] = mesh.normals[(i / 3) * 3 + 2];
            vertices.push_back(vertex);
        }

        MeshBuffers buffers;
        buffers.buffer = std::make_unique<crf::VulkanBuffer>(context, renderPass.getCommandPool());
        buffers.buffer->createVertexBuffer(vertices);
        buffers.buffer->createIndexBuffer(mesh.indices);
        meshBuffers.push_back(std::move(buffers));
    }

    crf::VulkanBuffer uniformBuffers(context, renderPass.getCommandPool());
    uniformBuffers.createUniformBuffers(1);

    crf::Log::info("Creating textures...");
    std::vector<std::unique_ptr<crf::VulkanTexture>> textures;
    for (const crf::ImageData& image : scene.getImages()) {
        textures.push_back(std::make_unique<crf::VulkanTexture>(
            context, renderPass.getCommandPool(), image.width, image.height, image.pixels));
    }
    if (textures.empty()) {
        std::vector<unsigned char> white(4, 255);
        textures.push_back(std::make_unique<crf::VulkanTexture>(context, renderPass.getCommandPool(), 1, 1, white));
    }

    std::vector<VkImageView> textureImageViews;
    std::vector<VkSampler> textureSamplers;
    textureImageViews.reserve(textures.size());
    textureSamplers.reserve(textures.size());
    for (const std::unique_ptr<crf::VulkanTexture>& texture : textures) {
        textureImageViews.push_back(texture->getImageView());
        textureSamplers.push_back(texture->getSampler());
    }

    crf::VulkanDescriptor descriptor(context, pipeline.getDescriptorSetLayout(), VK_NULL_HANDLE);
    descriptor.createDescriptorPool(static_cast<crf::u32>(textures.size()));
    descriptor.createDescriptorSets(uniformBuffers.getUniformBuffers(), 1, textureImageViews, textureSamplers);

    crf::UniformBufferObject ubo{};
    float aspect = static_cast<float>(context.getSwapChainExtent().width) / context.getSwapChainExtent().height;
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
    proj[1][1] *= -1.0f;
    std::memcpy(ubo.proj, glm::value_ptr(proj), sizeof(glm::mat4));

    float yaw = 45.0f;
    float pitch = 30.0f;
    float distance = 10.0f;

    std::vector<std::string> insideTriggers;

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
        distance = std::clamp(distance, 1.0f, 50.0f);

        const crf::Entity& player = scene.getEntity(playerEntity);
        const glm::vec3 playerPos(player.transform[3][0], player.transform[3][1], player.transform[3][2]);

        const std::vector<std::string> overlapped = scene.overlappingTriggers(player.bounds);
        for (const std::string& name : overlapped) {
            if (std::find(insideTriggers.begin(), insideTriggers.end(), name) == insideTriggers.end()) {
                crf::Log::info("Trigger activated: '{}'", name);
            }
        }
        insideTriggers = overlapped;

        float yawRad = glm::radians(yaw);
        float pitchRad = glm::radians(pitch);
        glm::vec3 eye(
            playerPos.x + distance * std::cos(pitchRad) * std::sin(yawRad),
            playerPos.y + distance * std::sin(pitchRad),
            playerPos.z + distance * std::cos(pitchRad) * std::cos(yawRad));
        glm::mat4 view = glm::lookAt(eye, playerPos, glm::vec3(0.0f, 1.0f, 0.0f));

        std::memcpy(ubo.view, glm::value_ptr(view), sizeof(glm::mat4));
        uniformBuffers.updateUniformBuffer(0, ubo);

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

            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.getSkyPipeline());

            VkBuffer skyVertexBuffers[] = {skyBuffers.getVertexBuffer()};
            VkDeviceSize skyOffsets[] = {0};
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, skyVertexBuffers, skyOffsets);

            VkDescriptorSet skyDescriptorSet = descriptor.getDescriptorSets()[0];
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.getPipelineLayout(),
                                    0, 1, &skyDescriptorSet, 0, nullptr);

            vkCmdDraw(commandBuffer, 36, 1, 0, 0);

            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.getGraphicsPipeline());

            for (const crf::Entity& entity : scene.getEntities()) {
                if (!entity.visible) {
                    continue;
                }

                vkCmdPushConstants(commandBuffer, pipeline.getPipelineLayout(),
                                   VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4),
                                   glm::value_ptr(entity.transform));

                const crf::MeshData& mesh = scene.getMeshes()[entity.meshIndex];
                const MeshBuffers& buffers = meshBuffers[entity.meshIndex];

                VkBuffer vertexBuffers[] = {buffers.buffer->getVertexBuffer()};
                VkDeviceSize offsets[] = {0};
                vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

                vkCmdBindIndexBuffer(commandBuffer, buffers.buffer->getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

                const crf::u32 textureOffset = scene.getTextureOffset(entity.meshIndex);
                for (crf::u32 p = entity.firstPrimitive; p < entity.firstPrimitive + entity.primitiveCount; p++) {
                    const crf::PrimitiveData& primitive = mesh.primitives[p];
                    VkDescriptorSet descriptorSet = descriptor.getDescriptorSets()[textureOffset + primitive.textureIndex];
                    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.getPipelineLayout(),
                                            0, 1, &descriptorSet, 0, nullptr);

                    vkCmdDrawIndexed(commandBuffer, primitive.indexCount, 1, primitive.firstIndex, 0, 0);
                }
            }
        });
    }

    crf::Log::info("Main loop ended, cleaning up...");
    crf::Log::info("Engine shutdown");
    crf::Log::shutdown();
    return 0;
}
