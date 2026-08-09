#pragma once

#include "core/Types.hpp"
#include "VulkanContext.hpp"
#include <string>

namespace crf {

class VulkanTexture {
public:
    VulkanTexture(VulkanContext& context, const std::string& filepath);
    ~VulkanTexture();

    VulkanTexture(const VulkanTexture&) = delete;
    VulkanTexture& operator=(const VulkanTexture&) = delete;
    VulkanTexture(VulkanTexture&&) = delete;
    VulkanTexture& operator=(VulkanTexture&&) = delete;

    VkImageView getImageView() const { return m_imageView; }
    VkSampler getSampler() const { return m_sampler; }

private:
    void createImage(u32 width, u32 height, VkFormat format, VkImageTiling tiling,
                     VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
                     VkImage& image, VkDeviceMemory& imageMemory);
    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
    void createSampler();

    VulkanContext& m_context;
    VkImage m_textureImage = nullptr;
    VkDeviceMemory m_textureImageMemory = nullptr;
    VkImageView m_textureImageView = nullptr;
    VkSampler m_textureSampler = nullptr;
};

}