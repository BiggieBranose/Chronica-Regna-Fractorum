#include <game/Game.hpp>

#include <core/Log.hpp>
#include <core/Types.hpp>
#include <core/Transform.hpp>
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
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace crf {

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

Game::Game(bool freeCamera) : m_freeCamera(freeCamera) {}

int Game::run() {
    Log::init("engine.log");
    Log::info("Engine v0.2.0 starting");

    WindowConfig wc;
    wc.title = "Chronica Regna Fractorum";
    wc.width = 1920;
    wc.height = 1080;
    wc.vsync = true;

    Window window(wc);

    Log::info("Creating VulkanContext...");
    VulkanContext context(window);

    VulkanRenderPass renderPass(context);
    Log::info("Creating render pass...");
    renderPass.createRenderPass();
    Log::info("Creating color resources...");
    renderPass.createColorResources();
    Log::info("Creating depth resources...");
    renderPass.createDepthResources();
    Log::info("Creating framebuffers...");
    renderPass.createFramebuffers();
    Log::info("Creating command pool...");
    renderPass.createCommandPool();
    Log::info("Creating command buffers...");
    renderPass.createCommandBuffers();
    Log::info("Creating sync objects...");
    renderPass.createSyncObjects();

    Log::info("Creating Graphics Pipeline...");
    VulkanPipeline pipeline(context, renderPass.getRenderPass(), renderPass.getMsaaSamples());
    pipeline.createDescriptorSetLayout();
    pipeline.createPipelineLayout(pipeline.getDescriptorSetLayout());
    pipeline.createGraphicsPipeline("shaders/cube.vert.spv", "shaders/cube.frag.spv");
    pipeline.createSkyPipeline();

    Log::info("Creating sky cube...");
    VulkanBuffer skyBuffers(context, renderPass.getCommandPool());
    skyBuffers.createVertexBuffer(makeSkyCube());

    Log::info("Building scene...");
    Scene scene;
    scene.loadSceneFile("assets/models/test_scene.glb");
    const crf::u32 playerMesh = scene.loadMeshFile("assets/models/test_cube.glb");

    Transform playerTransform;
    playerTransform.position = glm::vec3(0.0f, 0.5f, 0.0f);
    playerTransform.scale = glm::vec3(0.4f);
    const crf::u32 playerEntity = scene.addEntity("Player", playerMesh, playerTransform.toMat4());

    std::vector<MeshBuffers> meshBuffers;
    for (const MeshData& mesh : scene.getMeshes()) {
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
        buffers.buffer = std::make_unique<VulkanBuffer>(context, renderPass.getCommandPool());
        buffers.buffer->createVertexBuffer(vertices);
        buffers.buffer->createIndexBuffer(mesh.indices);
        meshBuffers.push_back(std::move(buffers));
    }

    VulkanBuffer uniformBuffers(context, renderPass.getCommandPool());
    uniformBuffers.createUniformBuffers(1);

    Log::info("Creating textures...");
    std::vector<std::unique_ptr<VulkanTexture>> textures;
    for (const ImageData& image : scene.getImages()) {
        textures.push_back(std::make_unique<VulkanTexture>(
            context, renderPass.getCommandPool(), image.width, image.height, image.pixels));
    }
    if (textures.empty()) {
        std::vector<unsigned char> white(4, 255);
        textures.push_back(std::make_unique<VulkanTexture>(context, renderPass.getCommandPool(), 1, 1, white));
    }

    std::vector<VkImageView> textureImageViews;
    std::vector<VkSampler> textureSamplers;
    textureImageViews.reserve(textures.size());
    textureSamplers.reserve(textures.size());
    for (const std::unique_ptr<VulkanTexture>& texture : textures) {
        textureImageViews.push_back(texture->getImageView());
        textureSamplers.push_back(texture->getSampler());
    }

    VulkanDescriptor descriptor(context, pipeline.getDescriptorSetLayout(), VK_NULL_HANDLE);
    descriptor.createDescriptorPool(static_cast<crf::u32>(textures.size()));
    descriptor.createDescriptorSets(uniformBuffers.getUniformBuffers(), 1, textureImageViews, textureSamplers);

    UniformBufferObject ubo{};
    float aspect = static_cast<float>(context.getSwapChainExtent().width) / context.getSwapChainExtent().height;
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
    proj[1][1] *= -1.0f;
    std::memcpy(ubo.proj, glm::value_ptr(proj), sizeof(glm::mat4));

    float yaw = 45.0f;
    float pitch = 30.0f;
    float distance = 10.0f;

    Log::info("Camera: {} follow (pass --camera to enable free camera)",
        m_freeCamera ? "free" : "fixed");

    std::vector<std::string> insideTriggers;

    Log::info("Entering main loop... (Press ESC to exit)");

    while (!window.shouldClose()) {
        window.pollEvents();

        if (window.isKeyJustPressed(GLFW_KEY_ESCAPE)) {
            break;
        }

        if (window.wasResized()) {
            renderPass.setFramebufferResized(true);
            window.clearResized();
        }

        if (m_freeCamera) {
            if (window.isMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT)) {
                yaw += window.getMouseDeltaX() * 0.01f;
                pitch += window.getMouseDeltaY() * 0.01f;
                pitch = std::clamp(pitch, -89.0f, 89.0f);
            }

            distance *= std::pow(0.95f, window.getScrollDelta());
            distance = std::clamp(distance, 1.0f, 50.0f);
        }

        const Entity& player = scene.getEntity(playerEntity);
        const glm::vec3 playerPos(player.transform[3][0], player.transform[3][1], player.transform[3][2]);

        const std::vector<std::string> overlapped = scene.getPhysics().overlappingTriggers(player.bounds);
        for (const std::string& name : overlapped) {
            if (std::find(insideTriggers.begin(), insideTriggers.end(), name) == insideTriggers.end()) {
                Log::info("Trigger activated: '{}'", name);
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

            for (const Entity& entity : scene.getEntities()) {
                if (!entity.visible) {
                    continue;
                }

                vkCmdPushConstants(commandBuffer, pipeline.getPipelineLayout(),
                                   VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4),
                                   glm::value_ptr(entity.transform));

                const MeshData& mesh = scene.getMeshes()[entity.meshIndex];
                const MeshBuffers& buffers = meshBuffers[entity.meshIndex];

                VkBuffer vertexBuffers[] = {buffers.buffer->getVertexBuffer()};
                VkDeviceSize offsets[] = {0};
                vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

                vkCmdBindIndexBuffer(commandBuffer, buffers.buffer->getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

                const crf::u32 textureOffset = scene.getTextureOffset(entity.meshIndex);
                for (crf::u32 p = entity.firstPrimitive; p < entity.firstPrimitive + entity.primitiveCount; p++) {
                    const PrimitiveData& primitive = mesh.primitives[p];
                    VkDescriptorSet descriptorSet = descriptor.getDescriptorSets()[textureOffset + primitive.textureIndex];
                    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.getPipelineLayout(),
                                            0, 1, &descriptorSet, 0, nullptr);

                    vkCmdDrawIndexed(commandBuffer, primitive.indexCount, 1, primitive.firstIndex, 0, 0);
                }
            }
        });
    }

    Log::info("Main loop ended, cleaning up...");
    Log::info("Engine shutdown");
    Log::shutdown();
    return 0;
}

}
