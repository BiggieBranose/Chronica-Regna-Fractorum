#pragma once

#include "VulkanBuffer.hpp"
#include "vulkan/vulkan.hpp"
#include <cstdint>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_raii.hpp>
#include <imgui/imgui.h>
#include <glm/glm.hpp>

namespace crf {
class ImGuiVulkanUtil{
public:
    ImGuiVulkanUtil(vk::raii::Device& device, vk::raii::PhysicalDevice& physicalDevice,
        vk::raii::Queue& graphicsQueue, uint32_t graphicsQueueFamily);
    ~ImGuiVulkanUtil();

    void init(float width, float height);
    void initResources();
    void setStyle(uint32_t index);
    void updateTexture(ImTextureData* tex);

    bool newFrame();
    void updateBuffers();
    void drawFrame(vk::raii::CommandBuffer& commandBuffer);

    void handleKey(int key, int scancode, int action, int mods);
    void handleMousePos(float x, float y);
    void handleMouseButton(int button, bool pressed);
    bool getWantKeyCapture();
    void charPressed(uint32_t key);

private:
    vk::raii::Sampler sampler{nullptr};
    VkBuffer vertexBuffer;
    VkBuffer indexBuffer;
    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;
    VkImage fontImage;
    VkImageView fontImageView;

    vk::raii::PipelineCache pipelineCache{nullptr};
    vk::raii::PipelineLayout pipelineLayout{nullptr};
    vk::raii::Pipeline pipeline{nullptr};
    vk::raii::DescriptorPool descriptorPool{nullptr};
    vk::raii::DescriptorSetLayout descriptorSetLayout{nullptr};
    vk::raii::DescriptorSet descriptorSet{nullptr};

    vk::raii::Device* device=nullptr;
    vk::raii::PhysicalDevice* physicalDevice=nullptr;
    vk::raii::Queue* graphicsQueue = nullptr;
    uint32_t graphicsQueueFamily = 0;

    ImGuiStyle vulkanStyle;

    struct PushConstBlock{
        glm::vec2 scale;
        glm::vec2 translate;
    } pushConstBlock;

    bool needsUpdateBuffers = false;

    vk::PipelineRenderingCreateInfo renderingInfo{};
    vk::Format colorFormat = vk::Format::eB8G8R8A8Unorm;
};
}