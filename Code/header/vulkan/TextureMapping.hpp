#pragma once

#include "../../header/vulkan/Buffers.hpp"
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
        void initialize();
        void createTextureImage();

        private:
        vk::raii::Image textureImage = nullptr;
        vk::raii::DeviceMemory textureImageMemory = nullptr;
    };
}