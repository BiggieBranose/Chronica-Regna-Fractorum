#pragma once

#include <vulkan/vulkan_raii.hpp>

namespace vkapp
{
    class VulkanDevice;

    vk::raii::CommandBuffer beginSingleTimeCommands(
        VulkanDevice& device, vk::raii::CommandPool& commandPool);

    void endSingleTimeCommands(
        VulkanDevice& device, vk::raii::CommandBuffer&& commandBuffer);
}
