#pragma once

#include "core/Types.hpp"
#include "VulkanContext.hpp"
#include "VulkanBuffer.hpp"
#include <string>

namespace crf {

class VulkanTexture {
public:
    VulkanTexture(VulkanContext& context, VkCommandPool commandPool);
    ~VulkanTexture();

    VulkanTexture(const VulkanTexture&) = delete;
    VulkanTexture& operator=(const VulkanTexture&) = delete;
    VulkanTexture(VulkanTexture&&) = delete;
    VulkanTexture& operator=(VulkanTexture&&) = delete;

    void loadTexture(const std::string& filepath);
    void createTextureImageView();
    void createTextureSampler();

    void createImage(u32 width, u32 height, u32 mipLevels, VkSampleCountFlagBits numSamples,
                     VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
                     VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, u32 mipLevels);
    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, u32 mipLevels);
    void copyBufferToImage(VkBuffer buffer, VkImage image, u32 width, u32 height);
    void generateMipmaps(VkImage image, VkFormat format, i32 texWidth, i32 texHeight, u32 mipLevels);

    VkImage getImage() const { return m_textureImage; }
    VkImageView getImageView() const { return m_textureImageView; }
    VkSampler getSampler() const { return m_textureSampler; }

private:
    VulkanContext& m_context;
    VkCommandPool m_commandPool;
    VkImage m_textureImage = nullptr;
    VkDeviceMemory m_textureImageMemory = nullptr;
    VkImageView m_textureImageView = nullptr;
    VkSampler m_textureSampler = nullptr;
    u32 m_mipLevels = 0;
};

}
