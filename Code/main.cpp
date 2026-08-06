#include <core/Log.hpp>
#include <graphics/Window.hpp>
#include <graphics/VulkanContext.hpp>
#include <graphics/VulkanRenderPass.hpp>
#include <graphics/GlTFLoader.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

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

    crf::Log::info("Loading glTF scene...");
    crf::loadScene("assets/models/test.glb");

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

        renderPass.drawFrame([](VkCommandBuffer, crf::u32) {});
    }

    crf::Log::info("Main loop ended, cleaning up...");
    crf::Log::info("Engine shutdown");
    crf::Log::shutdown();
    return 0;
}
