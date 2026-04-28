#pragma once

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>
#include "../../external/VMA/vk_mem_alloc.h"

#include <vector>
#include <string>

namespace vkapp
{
    class VulkanInstance; // forward declaration

    class VulkanDevice
    {
    public:
        VulkanDevice() = default;
        ~VulkanDevice() = default;

        VulkanDevice(const VulkanDevice&) = delete;
        VulkanDevice& operator=(const VulkanDevice&) = delete;

        void initialize(VulkanInstance& instance);
        void cleanup();

        // Accessors
        vk::raii::PhysicalDevice&       getPhysicalDevice()       { return m_physicalDevice; }
        const vk::raii::PhysicalDevice& getPhysicalDevice() const { return m_physicalDevice; }

        vk::raii::Device&       getDevice()       { return m_device; }
        const vk::raii::Device& getDevice() const { return m_device; }

        vk::raii::Queue&       getGraphicsQueue()       { return m_graphicsQueue; }
        const vk::raii::Queue& getGraphicsQueue() const { return m_graphicsQueue; }

        uint32_t getGraphicsQueueFamilyIndex() const { return m_graphicsQueueFamilyIndex; }

        VmaAllocator getAllocator() const { return m_allocator; }

    private:
        // internal helpers
        bool isDeviceSuitable(const vk::raii::PhysicalDevice& pd, vk::SurfaceKHR const& surface);
        void pickPhysicalDevice(VulkanInstance& instance);
        void createLogicalDevice(VulkanInstance& instance);
        void createAllocator(VulkanInstance& instance);

    private:
        // required device extensions
        const std::vector<const char*> m_requiredDeviceExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
        };

        vk::raii::PhysicalDevice m_physicalDevice = nullptr;
        vk::raii::Device         m_device         = nullptr;
        vk::raii::Queue          m_graphicsQueue  = nullptr;

        uint32_t m_graphicsQueueFamilyIndex = 0;

        VmaAllocator m_allocator = VK_NULL_HANDLE;
    };
}
