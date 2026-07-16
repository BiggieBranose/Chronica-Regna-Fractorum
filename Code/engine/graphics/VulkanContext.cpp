#include "VulkanContext.hpp"
#include "Window.hpp"
#include "core/Log.hpp"
#include "core/Assert.hpp"

#include <cstring>
#include <set>
#include <map>
#include <algorithm>
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

namespace crf {

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {
    (void)type;
    (void)pUserData;

    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        Log::warn("Vulkan validation: {}", pCallbackData->pMessage);
    }
    return VK_FALSE;
}

VulkanContext::VulkanContext(Window& window) : m_window(window) {
    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapChain();
    createImageViews();
}

VulkanContext::~VulkanContext() {
    cleanupSwapChain();
    vkDestroyDevice(m_device, nullptr);
    if (enableValidationLayers) {
        destroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
    }
    vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    vkDestroyInstance(m_instance, nullptr);
}

void VulkanContext::createInstance() {
    if (enableValidationLayers) {
        u32 layerCount = 0;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for (const char* layerName : validationLayers) {
            bool found = false;
            for (const auto& layer : availableLayers) {
                if (std::strcmp(layerName, layer.layerName) == 0) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                CRF_ASSERT_MSG(false, "Validation layer not available");
                return;
            }
        }
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Chronica Regna Fractorum";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.pEngineName = "CRF";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.apiVersion = VK_API_VERSION_1_2;

    u32 glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    if (enableValidationLayers) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<u32>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<u32>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();

        debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                     VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                     VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugCreateInfo.pfnUserCallback = debugCallback;
        createInfo.pNext = &debugCreateInfo;
    } else {
        createInfo.enabledLayerCount = 0;
        createInfo.pNext = nullptr;
    }

    VkResult result = vkCreateInstance(&createInfo, nullptr, &m_instance);
    CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to create Vulkan instance");
}

void VulkanContext::setupDebugMessenger() {
    if (!enableValidationLayers) return;

    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;

    VkResult result = createDebugUtilsMessengerEXT(m_instance, &createInfo, nullptr, &m_debugMessenger);
    CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to set up debug messenger");
}

void VulkanContext::createSurface() {
    VkResult result = glfwCreateWindowSurface(m_instance, m_window.getHandle(), nullptr, &m_surface);
    CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to create window surface");
}

void VulkanContext::pickPhysicalDevice() {
    u32 deviceCount = 0;
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
    CRF_ASSERT_MSG(deviceCount > 0, "Failed to find GPUs with Vulkan support");

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

    for (const auto& device : devices) {
        if (isDeviceSuitable(device)) {
            m_physicalDevice = device;
            m_msaaSamples = getMaxUsableSampleCount();
            break;
        }
    }

    CRF_ASSERT_MSG(m_physicalDevice != nullptr, "Failed to find a suitable GPU");
    Log::info("Physical device selected");
}

void VulkanContext::createLogicalDevice() {
    QueueFamilyIndices indices = findQueueFamilies(m_physicalDevice);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<u32> uniqueQueueFamilies = {
        indices.graphicsFamily.value(),
        indices.presentFamily.value()
    };

    if (indices.computeFamily.has_value()) {
        uniqueQueueFamilies.insert(indices.computeFamily.value());
    }

    float queuePriority = 1.0f;
    for (u32 queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.samplerAnisotropy = VK_TRUE;
    deviceFeatures.sampleRateShading = VK_TRUE;
    deviceFeatures.fillModeNonSolid = VK_TRUE;

    VkPhysicalDeviceVulkan12Features deviceFeatures12{};
    deviceFeatures12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    deviceFeatures12.bufferDeviceAddress = VK_TRUE;

    std::vector<const char*> extensions(m_deviceExtensions.begin(), m_deviceExtensions.end());

    u32 extCount = 0;
    vkEnumerateDeviceExtensionProperties(m_physicalDevice, nullptr, &extCount, nullptr);
    std::vector<VkExtensionProperties> availableExts(extCount);
    vkEnumerateDeviceExtensionProperties(m_physicalDevice, nullptr, &extCount, availableExts.data());

    std::set<std::string> availableExtNames;
    for (const auto& ext : availableExts) {
        availableExtNames.insert(ext.extensionName);
    }

    bool hasAccelStruct = availableExtNames.count(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
    bool hasRayTracingPipeline = availableExtNames.count(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
    bool hasRayQuery = availableExtNames.count(VK_KHR_RAY_QUERY_EXTENSION_NAME);
    bool hasDeferredHostOps = availableExtNames.count(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);

    m_hasRaytracing = hasAccelStruct && hasRayTracingPipeline && hasRayQuery && hasDeferredHostOps;

    void* featureChain = &deviceFeatures12;

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingFeatures{};
    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelStructFeatures{};

    if (m_hasRaytracing) {
        rayTracingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
        rayTracingFeatures.rayTracingPipeline = VK_TRUE;
        rayTracingFeatures.pNext = featureChain;
        featureChain = &rayTracingFeatures;

        accelStructFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
        accelStructFeatures.accelerationStructure = VK_TRUE;
        accelStructFeatures.pNext = featureChain;
        featureChain = &accelStructFeatures;

        extensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
        extensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
        extensions.push_back(VK_KHR_RAY_QUERY_EXTENSION_NAME);
        extensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);

        Log::info("Raytracing extensions available, enabling");
    } else {
        Log::warn("Raytracing not available, rasterization only");
    }

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<u32>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = static_cast<u32>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.pNext = featureChain;

    VkResult result = vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device);
    CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to create logical device");

    vkGetDeviceQueue(m_device, indices.graphicsFamily.value(), 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, indices.presentFamily.value(), 0, &m_presentQueue);
    if (indices.computeFamily.has_value()) {
        vkGetDeviceQueue(m_device, indices.computeFamily.value(), 0, &m_computeQueue);
    } else {
        m_computeQueue = m_graphicsQueue;
    }

    Log::info("Logical device created");
}

void VulkanContext::createSwapChain() {
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(m_physicalDevice);

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

    u32 imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = m_surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    QueueFamilyIndices indices = findQueueFamilies(m_physicalDevice);
    u32 queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    VkResult result = vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapChain);
    CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to create swap chain");

    vkGetSwapchainImagesKHR(m_device, m_swapChain, &imageCount, nullptr);
    m_swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(m_device, m_swapChain, &imageCount, m_swapChainImages.data());

    m_swapChainImageFormat = surfaceFormat.format;
    m_swapChainExtent = extent;

    Log::info("Swap chain created: {}x{}", extent.width, extent.height);
}

void VulkanContext::createImageViews() {
    m_swapChainImageViews.resize(m_swapChainImages.size());

    for (u32 i = 0; i < m_swapChainImages.size(); i++) {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = m_swapChainImages[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = m_swapChainImageFormat;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        VkResult result = vkCreateImageView(m_device, &createInfo, nullptr, &m_swapChainImageViews[i]);
        CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to create image view");
    }
}

void VulkanContext::recreateSwapChain() {
    int width = 0, height = 0;
    glfwGetFramebufferSize(m_window.getHandle(), &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(m_window.getHandle(), &width, &height);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(m_device);
    cleanupSwapChain();
    createSwapChain();
    createImageViews();
}

void VulkanContext::cleanupSwapChain() {
    for (auto imageView : m_swapChainImageViews) {
        vkDestroyImageView(m_device, imageView, nullptr);
    }
    m_swapChainImageViews.clear();

    if (m_swapChain) {
        vkDestroySwapchainKHR(m_device, m_swapChain, nullptr);
        m_swapChain = nullptr;
    }
}

bool VulkanContext::isDeviceSuitable(VkPhysicalDevice device) const {
    QueueFamilyIndices indices = findQueueFamilies(device);

    bool extensionsSupported = checkDeviceExtensionSupport(device);
    bool swapChainAdequate = false;
    if (extensionsSupported) {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }

    VkPhysicalDeviceFeatures supportedFeatures;
    vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

    return indices.isComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
}

QueueFamilyIndices VulkanContext::findQueueFamilies(VkPhysicalDevice device) const {
    QueueFamilyIndices indices;

    u32 queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    int i = 0;
    for (const auto& queueFamily : queueFamilies) {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &presentSupport);
        if (presentSupport) {
            indices.presentFamily = i;
        }

        if ((queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) && !(queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
            indices.computeFamily = i;
        }

        if (indices.isComplete()) break;
        i++;
    }

    if (!indices.computeFamily.has_value() && indices.graphicsFamily.has_value()) {
        indices.computeFamily = indices.graphicsFamily;
    }

    return indices;
}

bool VulkanContext::checkDeviceExtensionSupport(VkPhysicalDevice device) const {
    u32 extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> available(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, available.data());

    std::set<std::string> required(m_deviceExtensions.begin(), m_deviceExtensions.end());
    for (const auto& extension : available) {
        required.erase(extension.extensionName);
    }

    return required.empty();
}

SwapChainSupportDetails VulkanContext::querySwapChainSupport(VkPhysicalDevice device) const {
    SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &details.capabilities);

    u32 formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, nullptr);
    if (formatCount) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, details.formats.data());
    }

    u32 presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, nullptr);
    if (presentModeCount) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

VkSurfaceFormatKHR VulkanContext::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available) const {
    for (const auto& availableFormat : available) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
            availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }
    return available[0];
}

VkPresentModeKHR VulkanContext::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& available) const {
    for (const auto& availableMode : available) {
        if (availableMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availableMode;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanContext::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const {
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    }

    int width, height;
    glfwGetFramebufferSize(m_window.getHandle(), &width, &height);

    VkExtent2D actualExtent = {
        static_cast<u32>(width),
        static_cast<u32>(height)
    };

    actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

    return actualExtent;
}

VkFormat VulkanContext::findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) const {
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(m_physicalDevice, format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
            return format;
        }
        if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }

    CRF_ASSERT_MSG(false, "Failed to find supported format");
    return VK_FORMAT_UNDEFINED;
}

VkFormat VulkanContext::findDepthFormat() const {
    return findSupportedFormat(
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
}

bool VulkanContext::hasStencilComponent(VkFormat format) const {
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

VkResult VulkanContext::createDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pMessenger);
    }
    return VK_ERROR_EXTENSION_NOT_PRESENT;
}

void VulkanContext::destroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT messenger, const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, messenger, pAllocator);
    }
}

VkSampleCountFlagBits VulkanContext::getMaxUsableSampleCount() const {
    VkPhysicalDeviceProperties physicalDeviceProperties;
    vkGetPhysicalDeviceProperties(m_physicalDevice, &physicalDeviceProperties);

    VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts &
                                physicalDeviceProperties.limits.framebufferDepthSampleCounts;

    if (counts & VK_SAMPLE_COUNT_64_BIT) return VK_SAMPLE_COUNT_64_BIT;
    if (counts & VK_SAMPLE_COUNT_32_BIT) return VK_SAMPLE_COUNT_32_BIT;
    if (counts & VK_SAMPLE_COUNT_16_BIT) return VK_SAMPLE_COUNT_16_BIT;
    if (counts & VK_SAMPLE_COUNT_8_BIT) return VK_SAMPLE_COUNT_8_BIT;
    if (counts & VK_SAMPLE_COUNT_4_BIT) return VK_SAMPLE_COUNT_4_BIT;
    if (counts & VK_SAMPLE_COUNT_2_BIT) return VK_SAMPLE_COUNT_2_BIT;
    return VK_SAMPLE_COUNT_1_BIT;
}

}
