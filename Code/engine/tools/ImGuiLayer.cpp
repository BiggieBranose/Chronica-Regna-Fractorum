#include "ImGuiLayer.hpp"
#include "../core/Log.hpp"
#include "../core/Assert.hpp"
#include "../Engine.hpp"
#include "../input/Input.hpp"
#include "rendering/Renderer.hpp"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

namespace crf {

ImGuiLayer::~ImGuiLayer() { shutdown(); }

bool ImGuiLayer::initialize(Engine* engine) {
    m_engine = engine;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    auto& renderer = engine->getRenderer();

    ImGui_ImplGlfw_InitForVulkan(engine->getInput().getWindow(), true);

    VkFormat colorFormat = static_cast<VkFormat>(renderer.getColorFormat());
    VkPipelineRenderingCreateInfoKHR pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    pipelineInfo.colorAttachmentCount = 1;
    pipelineInfo.pColorAttachmentFormats = &colorFormat;

    ImGui_ImplVulkan_InitInfo info{};
    info.Instance = renderer.getInstance();
    info.PhysicalDevice = renderer.getPhysicalDevice();
    info.Device = renderer.getDevice();
    info.QueueFamily = renderer.getQueueIndex();
    info.Queue = *renderer.getQueue();
    info.DescriptorPool = renderer.getImGuiDescriptorPool();
    info.MinImageCount = 2;
    info.ImageCount = renderer.getImageCount();
    info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    info.UseDynamicRendering = true;
    info.PipelineRenderingCreateInfo = pipelineInfo;

    if (!ImGui_ImplVulkan_Init(&info)) {
        Log::error("Failed to initialize ImGui Vulkan backend");
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        return false;
    }

    if (!ImGui_ImplVulkan_CreateFontsTexture()) {
        Log::error("Failed to create ImGui font texture");
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        return false;
    }

    Log::info("ImGuiLayer initialized");
    m_initialized = true;
    return true;
}

void ImGuiLayer::shutdown() {
    if (m_initialized) {
        if (m_engine) {
            auto& renderer = m_engine->getRenderer();
            renderer.getQueue().waitIdle();
        }
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        Log::info("ImGuiLayer shutdown");
        m_initialized = false;
    }
}

void ImGuiLayer::beginFrame() {
    if (!m_initialized) return;
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiLayer::endFrame(vk::raii::CommandBuffer& cmd) {
    if (!m_initialized) return;

    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), *cmd);
}

} // namespace crf
