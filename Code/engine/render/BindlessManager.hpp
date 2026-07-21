#pragma once

#include "core/Types.hpp"
#include "graphics/VulkanContext.hpp"
#include <vulkan/vulkan.h>
#include <array>
#include <vector>

namespace crf {

class BindlessDescriptorManager {
public:
    static constexpr uint32_t MAX_TEXTURES = 4096;
    static constexpr uint32_t MAX_BUFFERS = 1024;

    struct TextureSlot {
        VkImageView view = VK_NULL_HANDLE;
        VkSampler sampler = VK_NULL_HANDLE;
        VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    };

    struct BufferSlot {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceSize offset = 0;
        VkDeviceSize range = VK_WHOLE_SIZE;
    };

    BindlessDescriptorManager(VulkanContext& context);
    ~BindlessDescriptorManager();

    BindlessDescriptorManager(const BindlessDescriptorManager&) = delete;
    BindlessDescriptorManager& operator=(const BindlessDescriptorManager&) = delete;
    BindlessDescriptorManager(BindlessDescriptorManager&&) = delete;
    BindlessDescriptorManager& operator=(BindlessDescriptorManager&&) = delete;

    void init();
    void cleanup();

    uint32_t addTexture(VkImageView view, VkSampler sampler, VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    void removeTexture(uint32_t index);
    void updateTexture(uint32_t index, VkImageView view, VkSampler sampler, VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    uint32_t addBuffer(VkBuffer buffer, VkDeviceSize offset = 0, VkDeviceSize range = VK_WHOLE_SIZE);
    void removeBuffer(uint32_t index);
    void updateBuffer(uint32_t index, VkBuffer buffer, VkDeviceSize offset = 0, VkDeviceSize range = VK_WHOLE_SIZE);

    VkDescriptorSetLayout getLayout() const { return m_descriptorSetLayout; }
    VkDescriptorSet getDescriptorSet() const { return m_descriptorSet; }

private:
    VulkanContext& m_context;

    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;

    std::array<TextureSlot, MAX_TEXTURES> m_textures{};
    std::array<BufferSlot, MAX_BUFFERS> m_buffers{};
    std::vector<uint32_t> m_freeTextureSlots;
    std::vector<uint32_t> m_freeBufferSlots;
    uint32_t m_textureCount = 0;
    uint32_t m_bufferCount = 0;
    uint32_t m_maxTextures = MAX_TEXTURES;
    uint32_t m_maxBuffers = MAX_BUFFERS;

    VkDescriptorSetLayout createLayout();
    void updateDescriptorSet();
    void updateTexture(uint32_t index);
    void updateBuffer(uint32_t index);
};

}