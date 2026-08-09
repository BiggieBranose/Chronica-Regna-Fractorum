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
    // BUG: vertexBuffer/indexBuffer are raw Vulkan C handles (VkBuffer = VkBuffer_T*), but the code
    // uses them as objects: constructor args in the ctor init list, map()/unmap()/getHandle() in
    // updateBuffers(). No such wrapper class exists (VulkanBuffer.hpp defines a different class).
    // Intended: a buffer wrapper (like vk::raii::Buffer) holding the frame's ImGui geometry.
    //   vertexBuffer - GPU buffer for all ImDrawVert vertices uploaded this frame.
    //   indexBuffer  - GPU buffer for all ImDrawIdx indices uploaded this frame.
    VkBuffer vertexBuffer;
    VkBuffer indexBuffer;
    // vertexCount/indexCount - capacity (in elements) of the last allocation of the buffers above,
    // used to decide whether updateBuffers() must reallocate.
    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;
    // BUG: same problem as the buffers: fontImage/fontImageView are raw C handles, but updateTexture()
    // assigns via Image(*device, ...) / ImageView(...) - types that don't exist anywhere in the project -
    // and calls fontImage.getHandle(). Intended: GPU image + image view for the font atlas texture,
    // uploaded from ImTextureData pixels.
    //   fontImage     - device-local image storing the font atlas bitmap (RGBA8 or R8).
    //   fontImageView - image view of fontImage used to sample it in the shader.
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

    // BUG: needsUpdateBuffers is set to true in newFrame() when vertex/index counts grow,
    // but nothing ever reads it - updateBuffers() is called unconditionally by the caller.
    // Intended: flag telling the caller "re-upload ImGui geometry this frame".
    bool needsUpdateBuffers = false;

    // BUG: renderingInfo carries the dynamic-rendering color format (set in the ctor) but is never
    // consumed: no pipeline is ever created, and drawFrame() shadows it with a local vk::RenderingInfo
    // of the same name (different type). Intended: color-format info passed to createGraphicsPipeline()
    // when building the ImGui pipeline.
    vk::PipelineRenderingCreateInfo renderingInfo{};
    vk::Format colorFormat = vk::Format::eB8G8R8A8Unorm;
};
}