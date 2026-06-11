#pragma once

#include "../../header/vulkan/Device.hpp"
#include <stb_image.h>
#include <string>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace vkapp{
    class TexMap{
        public:
        TexMap() = default;
        ~TexMap() = default;
        void initialize(VulkanDevice& device, vk::raii::CommandPool& commandPool);
        void createTextureImage(VulkanDevice& device, vk::raii::CommandPool& commandPool);
        void createTextureImageView(VulkanDevice& device);
        void createTextureSampler(VulkanDevice& device);
        void copyBufferToImage(VulkanDevice& device, vk::raii::CommandPool& commandPool, vk::raii::Buffer& src, vk::raii::Image& dst, uint32_t width, uint32_t height);
        void transitionImageLayout(VulkanDevice& device, vk::raii::CommandPool& commandPool, vk::Image image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout);
        std::pair<vk::raii::Image, vk::raii::DeviceMemory> createImage(VulkanDevice& device, uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties);

        vk::raii::ImageView& getTextureImageView() { return textureImageView; }
        vk::raii::Sampler&   getTextureSampler()   { return textureSampler; }

        private:
        vk::raii::ImageView createImageView(VulkanDevice& device, vk::Image image, vk::Format format);

        vk::raii::Image textureImage = nullptr;
        vk::raii::DeviceMemory textureImageMemory = nullptr;
        vk::raii::ImageView textureImageView = nullptr;
        vk::raii::Sampler   textureSampler   = nullptr;
    };
}