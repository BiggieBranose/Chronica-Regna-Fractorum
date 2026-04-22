/**
 * @file main.cpp
 * @brief Minimal Vulkan + GLFW application using Vulkan-Hpp RAII wrappers.
 */

#include "vulkan/vulkan.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <ranges>
#include <stdexcept>
#include <vector>

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#   include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

constexpr uint32_t WIDTH  = 800;   ///< Default window width.
constexpr uint32_t HEIGHT = 600;   ///< Default window height.

/** @brief vector of the validation layers to use, KHRONOS is the default basicly */
const std::vector<char const*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

/**
 * @class HelloTriangleApplication
 * @brief Encapsulates the lifecycle of a minimal Vulkan application.
 *
 * This class handles:
 * - Window creation via GLFW
 * - Vulkan instance creation
 * - Swap chain creation
 * - Main event loop
 * - Cleanup of resources
 */
class HelloTriangleApplication
{
public:
    /// @brief Runs the full application lifecycle.
    /// Calls initialization, enters the main loop, and performs cleanup.
    void run()
    {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:
    // -------------------------------------------------------------------------
    // Members
    // -------------------------------------------------------------------------

    /// @brief Pointer to the GLFW window.
    GLFWwindow* window = nullptr;

    /// @brief Vulkan-Hpp RAII context.
    vk::raii::Context context;

    /// @brief Vulkan instance handle.
    vk::raii::Instance instance = nullptr;

    /// @brief Debug messenger instance handle.
    vk::raii::DebugUtilsMessengerEXT debugMSG = nullptr;

    /// @brief Window surface used for presentation.
    vk::raii::SurfaceKHR surface = nullptr;

    /// @brief Handle to the physical GPU device.
    vk::raii::PhysicalDevice phyDevice = nullptr;

    /// @brief Handle to the logical device (connection from Vulkan to the GPU).
    vk::raii::Device logDevice = nullptr;

    /// @brief Graphics + present queue.
    vk::raii::Queue gfxQueue = nullptr;

    /// @brief Swap chain used for presenting images to the window.
    vk::raii::SwapchainKHR swapChain = nullptr;

    /// @brief Images owned by the swap chain.
    std::vector<vk::Image> scImages;

    /// @brief Surface format used by the swap chain images.
    vk::SurfaceFormatKHR scFormat{};

    /// @brief Extent (resolution) of the swap chain images.
    vk::Extent2D scExtent{};

    /// @brief Required device extensions (swapchain).
    std::vector<const char*> requiredDeviceExtension = {
        vk::KHRSwapchainExtensionName
    };

    /// @brief Handle to the image views, which allows viewing/mapping of images.
    std::vector<vk::raii::ImageView> swapChainImageViews;

    // -------------------------------------------------------------------------
    // Swap chain support struct (tutorial places it inside the class)
    // -------------------------------------------------------------------------

    /**
     * @brief Holds all swap chain support details for a physical device.
     *
     * Includes:
     * - Basic surface capabilities
     * - Supported surface formats
     * - Supported present modes
     */
    struct SwapChainSupportDetails
    {
        vk::SurfaceCapabilitiesKHR        caps;
        std::vector<vk::SurfaceFormatKHR> formats;
        std::vector<vk::PresentModeKHR>   presentModes;
    };

    // -------------------------------------------------------------------------
    // Initialization
    // -------------------------------------------------------------------------

    /// @brief Initializes the GLFW window.
    /// Sets GLFW to not create an OpenGL context and disables resizing.
    void initWindow()
    {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        window = glfwCreateWindow(WIDTH, HEIGHT, "Spookiest Amdus Window", nullptr, nullptr);
    }

    /// @brief Initializes Vulkan components.
    /// Creates instance, debug messenger, surface, physical device, logical device, swap chain, graphics pipeline.
    void initVulkan()
    {
        createInst();
        setupDebugMessenger();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createImageViews();
        createGraphicsPipeline();
    }

    // -------------------------------------------------------------------------
    // Debug messenger
    // -------------------------------------------------------------------------

    /// @brief Sets up the Vulkan debug messenger.
    /// Only active when validation layers are enabled.
    void setupDebugMessenger()
    {
        if (!enableValidationLayers) return;

        vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eError
        );

        vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(
            vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
            vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
            vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation
        );

        vk::DebugUtilsMessengerCreateInfoEXT createInfo{};
        createInfo.messageSeverity = severityFlags;
        createInfo.messageType     = messageTypeFlags;
        createInfo.pfnUserCallback = debugCallback;

        debugMSG = instance.createDebugUtilsMessengerEXT(createInfo);
    }

    // -------------------------------------------------------------------------
    // Surface
    // -------------------------------------------------------------------------

    /// @brief Creates the Vulkan surface for the GLFW window.
    void createSurface()
    {
        VkSurfaceKHR rawSurface{};
        if (glfwCreateWindowSurface(*instance, window, nullptr, &rawSurface) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create window surface!");
        }
        surface = vk::raii::SurfaceKHR(instance, rawSurface);
    }

    // -------------------------------------------------------------------------
    // Physical device selection
    // -------------------------------------------------------------------------

    /// @brief Picks a suitable physical device (GPU).
    /// Prefers discrete GPUs and requires geometry shader support.
    void pickPhysicalDevice()
    {
        auto devices = instance.enumeratePhysicalDevices();
        if (devices.empty())
            throw std::runtime_error("failed to find GPUs with Vulkan support!");

        std::multimap<int, vk::raii::PhysicalDevice> candidates;

        for (auto const& pd : devices)
        {
            auto props = pd.getProperties();
            auto feats = pd.getFeatures();

            if (!feats.geometryShader)
                continue;

            int score = 0;
            if (props.deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
                score += 1000;

            score += props.limits.maxImageDimension2D;

            candidates.insert({score, pd});
        }

        if (candidates.empty())
            throw std::runtime_error("failed to find a suitable GPU!");

        phyDevice = candidates.rbegin()->second;
    }

    // -------------------------------------------------------------------------
    // Logical device
    // -------------------------------------------------------------------------

    /// @brief Creates the logical device and retrieves the graphics/present queue.
    /// Enables Vulkan 1.3 dynamic rendering and extended dynamic state features.
    void createLogicalDevice()
    {
        auto qfProps = phyDevice.getQueueFamilyProperties();

        uint32_t queueIndex = ~0u;
        for (uint32_t i = 0; i < qfProps.size(); i++)
        {
            if ((qfProps[i].queueFlags & vk::QueueFlagBits::eGraphics) &&
                phyDevice.getSurfaceSupportKHR(i, *surface))
            {
                queueIndex = i;
                break;
            }
        }

        if (queueIndex == ~0u)
            throw std::runtime_error("No queue supports graphics + present!");

        vk::StructureChain<
            vk::PhysicalDeviceFeatures2,
            vk::PhysicalDeviceVulkan13Features,
            vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
        > featureChain;

        auto& feats13 = featureChain.get<vk::PhysicalDeviceVulkan13Features>();
        auto& dynFeat = featureChain.get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();

        feats13.dynamicRendering = VK_TRUE;
        dynFeat.extendedDynamicState = VK_TRUE;

        float priority = 1.0f;

        vk::DeviceQueueCreateInfo qci{};
        qci.queueFamilyIndex = queueIndex;
        qci.queueCount       = 1;
        qci.pQueuePriorities = &priority;

        vk::DeviceCreateInfo dci{};
        dci.pNext                   = &featureChain.get<vk::PhysicalDeviceFeatures2>();
        dci.queueCreateInfoCount    = 1;
        dci.pQueueCreateInfos       = &qci;
        dci.enabledExtensionCount   = static_cast<uint32_t>(requiredDeviceExtension.size());
        dci.ppEnabledExtensionNames = requiredDeviceExtension.data();

        logDevice = vk::raii::Device(phyDevice, dci);
        gfxQueue  = vk::raii::Queue(logDevice, queueIndex, 0);
    }

    // -------------------------------------------------------------------------
    // Swap chain helpers
    // -------------------------------------------------------------------------

    /// @brief Queries swap chain support details for the selected physical device.
    /// Uses the current window surface to determine capabilities, formats, and present modes.
    SwapChainSupportDetails querySwapChainSupport()
    {
        SwapChainSupportDetails d{};
        d.caps         = phyDevice.getSurfaceCapabilitiesKHR(*surface);
        d.formats      = phyDevice.getSurfaceFormatsKHR(*surface);
        d.presentModes = phyDevice.getSurfacePresentModesKHR(*surface);
        return d;
    }

    /// @brief Chooses the best surface format for the swap chain.
    /// Prefers B8G8R8A8 SRGB with SRGB nonlinear color space if available.
    vk::SurfaceFormatKHR chooseSwapSurfaceFormat(
        std::vector<vk::SurfaceFormatKHR> const& formats)
    {
        auto it = std::ranges::find_if(
            formats,
            [](auto const& f) {
                return f.format == vk::Format::eB8G8R8A8Srgb &&
                       f.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
            });

        return it != formats.end() ? *it : formats[0];
    }

    /// @brief Chooses the best present mode for the swap chain.
    /// Prefers Mailbox if available, otherwise falls back to FIFO (which is guaranteed).
    vk::PresentModeKHR chooseSwapPresentMode(
        std::vector<vk::PresentModeKHR> const& modes)
    {
        bool hasMailbox = std::ranges::any_of(
            modes,
            [](auto m) { return m == vk::PresentModeKHR::eMailbox; });

        return hasMailbox ? vk::PresentModeKHR::eMailbox
                          : vk::PresentModeKHR::eFifo;
    }

    /// @brief Chooses the swap chain extent (resolution).
    /// Uses currentExtent if fixed, otherwise clamps framebuffer size to allowed range.
    vk::Extent2D chooseSwapExtent(vk::SurfaceCapabilitiesKHR const& caps)
    {
        if (caps.currentExtent.width != std::numeric_limits<uint32_t>::max())
            return caps.currentExtent;

        int w, h;
        glfwGetFramebufferSize(window, &w, &h);

        return vk::Extent2D{
            std::clamp<uint32_t>(w, caps.minImageExtent.width,  caps.maxImageExtent.width),
            std::clamp<uint32_t>(h, caps.minImageExtent.height, caps.maxImageExtent.height)
        };
    }

    /// @brief Chooses the minimum number of images in the swap chain.
    /// Requests at least 3 images, clamped to the implementation's maximum.
    uint32_t chooseSwapMinImageCount(vk::SurfaceCapabilitiesKHR const& caps)
    {
        uint32_t count = std::max(3u, caps.minImageCount);
        if (caps.maxImageCount > 0 && count > caps.maxImageCount)
            count = caps.maxImageCount;
        return count;
    }

    // -------------------------------------------------------------------------
    // Swap chain creation
    // -------------------------------------------------------------------------

    /// @brief Creates the swap chain and retrieves its images.
    /// Stores the chosen format and extent for later use.
    void createSwapChain()
    {
        auto support = querySwapChainSupport();

        scFormat = chooseSwapSurfaceFormat(support.formats);
        scExtent = chooseSwapExtent(support.caps);
        uint32_t minImages = chooseSwapMinImageCount(support.caps);
        vk::PresentModeKHR presentMode = chooseSwapPresentMode(support.presentModes);

        vk::SwapchainCreateInfoKHR sci{};
        sci.surface          = *surface;
        sci.minImageCount    = minImages;
        sci.imageFormat      = scFormat.format;
        sci.imageColorSpace  = scFormat.colorSpace;
        sci.imageExtent      = scExtent;
        sci.imageArrayLayers = 1;
        sci.imageUsage       = vk::ImageUsageFlagBits::eColorAttachment;
        sci.imageSharingMode = vk::SharingMode::eExclusive;
        sci.preTransform     = support.caps.currentTransform;
        sci.compositeAlpha   = vk::CompositeAlphaFlagBitsKHR::eOpaque;
        sci.presentMode      = presentMode;
        sci.clipped          = VK_TRUE;

        swapChain = vk::raii::SwapchainKHR(logDevice, sci);
        scImages  = swapChain.getImages();
    }

    // -------------------------------------------------------------------------
    // Main loop + cleanup
    // -------------------------------------------------------------------------

    /// @brief Main application loop.
    /// Polls window events until the user closes the window.
    void mainLoop()
    {
        while (!glfwWindowShouldClose(window))
            glfwPollEvents();
    }

    /// @brief Cleans up GLFW and Vulkan resources.
    /// Vulkan RAII objects clean themselves up automatically.
    void cleanup()
    {
        glfwDestroyWindow(window);
        glfwTerminate();
    }

    // -------------------------------------------------------------------------
    // Instance creation
    // -------------------------------------------------------------------------

    /// @brief Creates the Vulkan instance.
    /// Validates required layers and extensions before creation.
    void createInst()
    {
        constexpr vk::ApplicationInfo appInfo(
            "CRF",
            VK_MAKE_VERSION(1,0,0),
            "Branose Engine",
            VK_MAKE_VERSION(1,1,0),
            vk::ApiVersion14
        );

        std::vector<char const*> layers;
        if (enableValidationLayers)
            layers = validationLayers;

        auto layerProps = context.enumerateInstanceLayerProperties();
        for (auto const* req : layers)
        {
            bool found = std::ranges::any_of(
                layerProps,
                [req](auto const& lp) {
                    return std::strcmp(lp.layerName, req) == 0;
                });

            if (!found)
                throw std::runtime_error("Missing required validation layer!");
        }

        auto extensions = getRequiredInstanceExtensions();
        auto extProps   = context.enumerateInstanceExtensionProperties();

        for (auto const* req : extensions)
        {
            bool found = std::ranges::any_of(
                extProps,
                [req](auto const& ep) {
                    return std::strcmp(ep.extensionName, req) == 0;
                });

            if (!found)
                throw std::runtime_error("Missing required instance extension!");
        }

        vk::InstanceCreateInfo ici(
            {},
            &appInfo,
            static_cast<uint32_t>(layers.size()),
            layers.data(),
            static_cast<uint32_t>(extensions.size()),
            extensions.data()
        );

        instance = vk::raii::Instance(context, ici);
    }

    /// @brief Gets required instance extensions.
    /// Adds debug utils extension when validation layers are enabled.
    std::vector<const char*> getRequiredInstanceExtensions()
    {
        uint32_t count = 0;
        auto glfwExt = glfwGetRequiredInstanceExtensions(&count);

        std::vector<const char*> ext(glfwExt, glfwExt + count);
        if (enableValidationLayers)
            ext.push_back(vk::EXTDebugUtilsExtensionName);

        return ext;
    }

    /// @brief Creates image views for each swap chain image.
    /// Image views allow the swap chain images to be used as color attachments.
    void createImageViews()
    {
        assert(swapChainImageViews.empty());

        swapChainImageViews.reserve(scImages.size());

        vk::ImageViewCreateInfo ivci{};
        ivci.viewType = vk::ImageViewType::e2D;
        ivci.format   = scFormat.format;

        // Identity swizzle: R→R, G→G, B→B, A→A
        ivci.components = vk::ComponentMapping{
            vk::ComponentSwizzle::eIdentity,
            vk::ComponentSwizzle::eIdentity,
            vk::ComponentSwizzle::eIdentity,
            vk::ComponentSwizzle::eIdentity
        };

        // Use the whole image as a color attachment, no mipmaps, no array layers.
        ivci.subresourceRange = vk::ImageSubresourceRange{
            vk::ImageAspectFlagBits::eColor, // aspectMask
            0,                               // baseMipLevel
            1,                               // levelCount
            0,                               // baseArrayLayer
            1                                // layerCount
        };

        for (auto const& image : scImages)
        {
            ivci.image = image;
            swapChainImageViews.emplace_back(logDevice, ivci);
        }
    }

    void createGraphicsPipeline()
    {
        
    }

    // -------------------------------------------------------------------------
    // Debug callback
    // -------------------------------------------------------------------------

    /// @brief Vulkan debug messenger callback.
    /// Prints validation messages to stderr and never aborts the call.
    static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(
        vk::DebugUtilsMessageSeverityFlagBitsEXT,
        vk::DebugUtilsMessageTypeFlagsEXT,
        const vk::DebugUtilsMessengerCallbackDataEXT* data,
        void*)
    {
        std::cerr << "validation layer: " << data->pMessage << std::endl;
        return vk::False;
    }
};

// -----------------------------------------------------------------------------
// Entry point
// -----------------------------------------------------------------------------

/// @brief Application entry point.
/// Creates and runs the HelloTriangleApplication instance.
int main()
{
    try
    {
        HelloTriangleApplication app;
        app.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
