#include "../../header/vulkan/CommandHelpers.hpp"
#include "../../header/vulkan/Device.hpp"

namespace vkapp
{
    vk::raii::CommandBuffer beginSingleTimeCommands(
        VulkanDevice& device, vk::raii::CommandPool& commandPool)
    {
        vk::CommandBufferAllocateInfo allocInfo{};
        allocInfo.commandPool        = *commandPool;
        allocInfo.level              = vk::CommandBufferLevel::ePrimary;
        allocInfo.commandBufferCount = 1;

        vk::raii::CommandBuffer cb = std::move(
            vk::raii::CommandBuffers(device.getDevice(), allocInfo).front());

        vk::CommandBufferBeginInfo beginInfo{};
        beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
        cb.begin(beginInfo);

        return cb;
    }

    void endSingleTimeCommands(
        VulkanDevice& device, vk::raii::CommandBuffer&& commandBuffer)
    {
        commandBuffer.end();

        vk::SubmitInfo submitInfo{};
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers    = &*commandBuffer;

        device.getGraphicsQueue().submit(submitInfo, nullptr);
        device.getGraphicsQueue().waitIdle();
    }
}
