#pragma once

#include "../../header/vulkan/Buffers.hpp"
#include "../../header/vulkan/Device.hpp"
#include <stb_image.h>
#include <string>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#define STB_IMAGE_IMPLEMENTATION

namespace vkapp{
    class TexMap{
        public:
        TexMap() = default;
        ~TexMap() = default;
        void initialize(VulkanDevice& device, Buffers& buffer);
        void createTextureImage(VulkanDevice& device, Buffers& buffer);

        private:
        vk::raii::Image textureImage = nullptr;
        vk::raii::DeviceMemory textureImageMemory = nullptr;
    };
}