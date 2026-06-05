#pragma once

#include "../../header/vulkan/Buffers.hpp"
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
        void initialize(VulkanDevice& device, Buffers& buffer);
        void createTextureImage(VulkanDevice& device, Buffers& buffer);
        std::pair<vk::raii::Image, vk::raii::DeviceMemory> createImage(VulkanDevice& device, uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties);

        private:
        vk::raii::Image image = nullptr;
        vk::raii::DeviceMemory imageMemory = nullptr;
        vk::raii::Image textureImage = nullptr;
        vk::raii::DeviceMemory textureImageMemory = nullptr;
        vk::MemoryRequirements memRequirements;
        vk::MemoryAllocateInfo allocInfo;
    };
}