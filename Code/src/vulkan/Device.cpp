#include "../../header/vulkan/Device.hpp"
#include "../../header/vulkan/Instance.hpp"

#define VMA_IMPLEMENTATION
#include "../../external/VMA/vk_mem_alloc.h"

#include <stdexcept>
#include <cstring>
#include <iostream>

namespace vkapp
{
    void VulkanDevice::initialize(VulkanInstance& instance)
    {
        pickPhysicalDevice(instance);
        createLogicalDevice(instance);
        createAllocator(instance);
    }

    void VulkanDevice::cleanup()
    {
        if (m_allocator != VK_NULL_HANDLE)
        {
            vmaDestroyAllocator(m_allocator);
            m_allocator = VK_NULL_HANDLE;
        }

        m_device = nullptr;
        m_physicalDevice = nullptr;
    }

    bool VulkanDevice::isDeviceSuitable(const vk::raii::PhysicalDevice& pd, vk::SurfaceKHR const& surface)
    {
        bool supportsVulkan13 = pd.getProperties().apiVersion >= VK_API_VERSION_1_3;

        auto queueFamilies = pd.getQueueFamilyProperties();
        bool supportsGraphics = false;
        bool supportsPresent  = false;
        uint32_t index        = 0;

        for (auto const& qfp : queueFamilies)
        {
            if (qfp.queueFlags & vk::QueueFlagBits::eGraphics)
                supportsGraphics = true;

            if (pd.getSurfaceSupportKHR(index, surface))
                supportsPresent = true;

            ++index;
        }

        auto availableExtensions = pd.enumerateDeviceExtensionProperties();
        bool supportsAllExtensions = true;

        for (auto const* reqExt : m_requiredDeviceExtensions)
        {
            bool found = false;
            for (auto const& ext : availableExtensions)
            {
                if (strcmp(ext.extensionName, reqExt) == 0)
                {
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                supportsAllExtensions = false;
                break;
            }
        }

        auto features = pd.getFeatures2<
            vk::PhysicalDeviceFeatures2,
            vk::PhysicalDeviceVulkan11Features,
            vk::PhysicalDeviceVulkan13Features,
            vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();

        bool supportsRequiredFeatures =
            features.get<vk::PhysicalDeviceVulkan11Features>().shaderDrawParameters &&
            features.get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
            features.get<vk::PhysicalDeviceVulkan13Features>().synchronization2 &&
            features.get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState;

        return supportsVulkan13 &&
               supportsGraphics &&
               supportsPresent &&
               supportsAllExtensions &&
               supportsRequiredFeatures;
    }

    void VulkanDevice::pickPhysicalDevice(VulkanInstance& instance)
    {
        auto& vkInstance = instance.getInstance();
        auto& surface    = instance.getSurface();

        std::vector<vk::raii::PhysicalDevice> devices = vkInstance.enumeratePhysicalDevices();

        for (auto const& pd : devices)
        {
            if (isDeviceSuitable(pd, instance.getSurface()))
            {
                m_physicalDevice = pd;
                return;
            }
        }

        throw std::runtime_error("failed to find a suitable GPU!");
    }

    void VulkanDevice::createLogicalDevice(VulkanInstance& instance)
    {
        auto queueFamilyProperties = m_physicalDevice.getQueueFamilyProperties();

        uint32_t queueIndex = ~0u;
        for (uint32_t i = 0; i < queueFamilyProperties.size(); ++i)
        {
            if ((queueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eGraphics) &&
                m_physicalDevice.getSurfaceSupportKHR(i, *instance.getSurface()))
            {
                queueIndex = i;
                break;
            }
        }

        if (queueIndex == ~0u)
            throw std::runtime_error("Could not find a queue for graphics and present");

        m_graphicsQueueFamilyIndex = queueIndex;

        vk::PhysicalDeviceFeatures2 baseFeatures{};
        vk::PhysicalDeviceVulkan11Features f11{};
        vk::PhysicalDeviceVulkan13Features f13{};
        vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT fExt{};

        baseFeatures.pNext = &f11;
        f11.pNext          = &f13;
        f13.pNext          = &fExt;

        f11.shaderDrawParameters  = VK_TRUE;
        f13.dynamicRendering      = VK_TRUE;
        f13.synchronization2      = VK_TRUE;
        fExt.extendedDynamicState = VK_TRUE;

        float queuePriority = 1.0f;
        vk::DeviceQueueCreateInfo queueInfo{};
        queueInfo.queueFamilyIndex = queueIndex;
        queueInfo.queueCount       = 1;
        queueInfo.pQueuePriorities = &queuePriority;

        vk::DeviceCreateInfo deviceInfo{};
        deviceInfo.pNext                   = &baseFeatures;
        deviceInfo.queueCreateInfoCount    = 1;
        deviceInfo.pQueueCreateInfos       = &queueInfo;
        deviceInfo.enabledExtensionCount   = static_cast<uint32_t>(m_requiredDeviceExtensions.size());
        deviceInfo.ppEnabledExtensionNames = m_requiredDeviceExtensions.data();

        m_device = vk::raii::Device(m_physicalDevice, deviceInfo);
        m_graphicsQueue = vk::raii::Queue(m_device, queueIndex, 0);
    }

    void VulkanDevice::createAllocator(VulkanInstance& instance)
    {
        VmaAllocatorCreateInfo info{};
        info.instance       = *instance.getInstance();
        info.physicalDevice = *m_physicalDevice;
        info.device         = *m_device;
        info.vulkanApiVersion = VK_API_VERSION_1_3;

        if (vmaCreateAllocator(&info, &m_allocator) != VK_SUCCESS)
            throw std::runtime_error("failed to create VMA allocator");
    }

}
