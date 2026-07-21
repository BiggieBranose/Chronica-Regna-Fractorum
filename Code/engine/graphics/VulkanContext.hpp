#pragma once

#include "core/Types.hpp"
#include "core/Platform.hpp"
#include <string_view>
#include <vulkan/vulkan.h>

namespace crf {

class Window;

struct QueueFamilyIndices {
    std::optional<u32> graphicsFamily;
    std::optional<u32> presentFamily;
    std::optional<u32> computeFamily;

    bool isComplete() const {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities{};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

class VulkanContext {
public:
    explicit VulkanContext(Window& window);
    ~VulkanContext();

    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;
    VulkanContext(VulkanContext&&) = delete;
    VulkanContext& operator=(VulkanContext&&) = delete;

    void recreateSwapChain();
    void cleanupSwapChain();

    VkInstance getInstance() const { return m_instance; }
    VkDevice getDevice() const { return m_device; }
    VkPhysicalDevice getPhysicalDevice() const { return m_physicalDevice; }
    VkSurfaceKHR getSurface() const { return m_surface; }
    VkQueue getGraphicsQueue() const { return m_graphicsQueue; }
    VkQueue getPresentQueue() const { return m_presentQueue; }
    VkQueue getComputeQueue() const { return m_computeQueue; }
    VkSwapchainKHR getSwapChain() const { return m_swapChain; }
    VkFormat getSwapChainImageFormat() const { return m_swapChainImageFormat; }
    VkExtent2D getSwapChainExtent() const { return m_swapChainExtent; }
    const std::vector<VkImage>& getSwapChainImages() const { return m_swapChainImages; }
    const std::vector<VkImageView>& getSwapChainImageViews() const { return m_swapChainImageViews; }
    VkImageView getSwapChainImageView(u32 index) const { return m_swapChainImageViews[index]; }
    u32 getSwapChainImageCount() const { return static_cast<u32>(m_swapChainImages.size()); }
    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) const;
    VkFormat findDepthFormat() const;
    bool hasStencilComponent(VkFormat format) const;
    bool hasRaytracing() const { return m_hasRaytracing; }

    static constexpr u32 MAX_FRAMES_IN_FLIGHT = 2;

    // Raytracing function pointers
    PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR = nullptr;
    PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR = nullptr;
    PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR = nullptr;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR = nullptr;
    PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR = nullptr;
    PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR = nullptr;
    PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR = nullptr;
    PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR = nullptr;
    PFN_vkGetBufferDeviceAddress vkGetBufferDeviceAddress = nullptr;

    void loadRaytracingFunctions();

private:
    void createInstance();
    void setupDebugMessenger();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createSwapChain();
    void createImageViews();

    bool isDeviceSuitable(VkPhysicalDevice device) const;
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) const;
    bool checkDeviceExtensionSupport(VkPhysicalDevice device) const;
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) const;
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available) const;
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& available) const;
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const;
    VkSampleCountFlagBits getMaxUsableSampleCount() const;

    static VkResult createDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pMessenger);
    static void destroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT messenger, const VkAllocationCallbacks* pAllocator);

    Window& m_window;
    VkInstance m_instance = nullptr;
    VkDebugUtilsMessengerEXT m_debugMessenger = nullptr;
    VkSurfaceKHR m_surface = nullptr;
    VkPhysicalDevice m_physicalDevice = nullptr;
    VkDevice m_device = nullptr;
    VkQueue m_graphicsQueue = nullptr;
    VkQueue m_presentQueue = nullptr;
    VkQueue m_computeQueue = nullptr;
    VkSwapchainKHR m_swapChain = nullptr;
    std::vector<VkImage> m_swapChainImages;
    std::vector<VkImageView> m_swapChainImageViews;
    VkFormat m_swapChainImageFormat{};
    VkExtent2D m_swapChainExtent{};

    const std::vector<const char*> m_deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

#ifdef NDEBUG
    static constexpr bool enableValidationLayers = false;
#else
    static constexpr bool enableValidationLayers = true;
#endif

    static constexpr std::array<const char*, 1> validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };

    VkSampleCountFlagBits m_msaaSamples = VK_SAMPLE_COUNT_1_BIT;
    bool m_hasRaytracing = false;
};

}
