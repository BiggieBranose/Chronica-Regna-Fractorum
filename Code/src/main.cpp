#include <algorithm>
#include <assert.h>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#   include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

#define GLFW_INCLUDE_VULKAN        // REQUIRED only for GLFW CreateWindowSurface.
#include <GLFW/glfw3.h>

constexpr uint32_t WIDTH  = 800;
constexpr uint32_t HEIGHT = 600;

const std::vector<char const *> validationLayers = {
    "VK_LAYER_KHRONOS_validation"};

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

/**
 * @class HelloTriangleApplication
 * @brief Minimal Vulkan + GLFW application using Vulkan-Hpp RAII wrappers.
 *
 * This class encapsulates the lifecycle of a tiny Vulkan application:
 * - window creation (GLFW)
 * - instance and device creation (Vulkan-Hpp RAII)
 * - swapchain and image views
 * - shader module loading (SPIR-V produced by Slang)
 *
 * The file is intentionally verbose with Doxygen and inline comments to
 * document each step for learning and future extension.
 */
class HelloTriangleApplication
{
  public:
    /**
     * @brief Run the application: init, loop, cleanup.
     */
    void run()
    {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

  private:
    // ---------------------------------------------------------------------
    // Members (handles and state)
    // ---------------------------------------------------------------------
    GLFWwindow                      *window = nullptr;
    vk::raii::Context                context;
    vk::raii::Instance               instance       = nullptr;
    vk::raii::DebugUtilsMessengerEXT debugMessenger = nullptr;
    vk::raii::SurfaceKHR             surface        = nullptr;
    vk::raii::PhysicalDevice         physicalDevice = nullptr;
    vk::raii::Device                 device         = nullptr;
    vk::raii::Queue                  queue          = nullptr;
    vk::raii::SwapchainKHR           swapChain      = nullptr;
    std::vector<vk::Image>           swapChainImages;
    vk::SurfaceFormatKHR             swapChainSurfaceFormat;
    vk::Extent2D                     swapChainExtent;
    std::vector<vk::raii::ImageView> swapChainImageViews;

    /** Required device extensions (swapchain) */
    std::vector<const char *> requiredDeviceExtension = {
        vk::KHRSwapchainExtensionName};

    // ---------------------------------------------------------------------
    // Initialization: window + Vulkan
    // ---------------------------------------------------------------------

    /**
     * @brief Initialize GLFW window.
     *
     * Creates a window without an OpenGL context (GLFW_NO_API) and disables
     * resizing for simplicity in this tutorial.
     */
    void initWindow()
    {
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
    }

    /**
     * @brief Initialize Vulkan objects and prepare for rendering.
     *
     * This function orchestrates instance creation, debug messenger setup,
     * surface creation, physical/logical device selection, swapchain and
     * image view creation, and finally the graphics pipeline setup.
     */
    void initVulkan()
    {
        createInstance();
        setupDebugMessenger();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createImageViews();
        createGraphicsPipeline();
    }

    /**
     * @brief Main event loop.
     *
     * Polls GLFW events until the window should close.
     */
    void mainLoop()
    {
        while (!glfwWindowShouldClose(window))
        {
            glfwPollEvents();
        }
    }

    /**
     * @brief Cleanup GLFW resources.
     *
     * Vulkan RAII objects are destroyed automatically when the class
     * instance goes out of scope, but we still destroy the GLFW window and
     * terminate GLFW explicitly here.
     */
    void cleanup()
    {
        glfwDestroyWindow(window);

        glfwTerminate();
    }

    // ---------------------------------------------------------------------
    // Instance and debug
    // ---------------------------------------------------------------------

    /**
     * @brief Create the Vulkan instance.
     *
     * Validates that requested validation layers and instance extensions are
     * available before creating the vk::raii::Instance.
     */
    void createInstance()
    {
        vk::ApplicationInfo appInfo{};
        appInfo.pApplicationName   = "Hello Triangle";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName        = "No Engine";
        appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion         = vk::ApiVersion14;

        // Get the required layers
        std::vector<char const *> requiredLayers;
        if (enableValidationLayers)
        {
            requiredLayers.assign(validationLayers.begin(), validationLayers.end());
        }

        // Check if the required layers are supported by the Vulkan implementation.
        auto layerProperties = context.enumerateInstanceLayerProperties();
        for (auto const *requiredLayer : requiredLayers)
        {
            bool found = false;
            for (auto const &layerProperty : layerProperties)
            {
                if (std::strcmp(layerProperty.layerName, requiredLayer) == 0)
                {
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                throw std::runtime_error("Required layer not supported: " + std::string(requiredLayer));
            }
        }

        // Get the required extensions.
        auto requiredExtensions = getRequiredInstanceExtensions();

        // Check if the required extensions are supported by the Vulkan implementation.
        auto extensionProperties = context.enumerateInstanceExtensionProperties();
        for (auto const *requiredExtension : requiredExtensions)
        {
            bool found = false;
            for (auto const &extensionProperty : extensionProperties)
            {
                if (std::strcmp(extensionProperty.extensionName, requiredExtension) == 0)
                {
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                throw std::runtime_error("Required extension not supported: " + std::string(requiredExtension));
            }
        }

        vk::InstanceCreateInfo createInfo{};
        createInfo.pApplicationInfo        = &appInfo;
        createInfo.enabledLayerCount       = static_cast<uint32_t>(requiredLayers.size());
        createInfo.ppEnabledLayerNames     = requiredLayers.data();
        createInfo.enabledExtensionCount   = static_cast<uint32_t>(requiredExtensions.size());
        createInfo.ppEnabledExtensionNames = requiredExtensions.data();
        instance                           = vk::raii::Instance(context, createInfo);
    }

    /**
     * @brief Vulkan debug messenger callback.
     *
     * Prints validation messages to stderr and never aborts the call.
     */
    static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
                                                          vk::DebugUtilsMessageTypeFlagsEXT,
                                                          const vk::DebugUtilsMessengerCallbackDataEXT *pCallbackData,
                                                          void *)
    {
        if (severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eError ||
            severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning)
        {
            std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
        }

        return vk::False;
    }

    /**
     * @brief Setup the Vulkan debug messenger (if validation layers enabled).
     *
     * Registers a callback that prints warnings and errors from validation
     * layers to stderr.
     */
    void setupDebugMessenger()
    {
        if (!enableValidationLayers)
            return;

        vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
        vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(
            vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
            vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
            vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
        vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfoEXT{};
        debugUtilsMessengerCreateInfoEXT.messageSeverity = severityFlags;
        debugUtilsMessengerCreateInfoEXT.messageType     = messageTypeFlags;
        debugUtilsMessengerCreateInfoEXT.pfnUserCallback = &debugCallback;
        debugMessenger                                  = instance.createDebugUtilsMessengerEXT(debugUtilsMessengerCreateInfoEXT);
    }

    // ---------------------------------------------------------------------
    // Surface and device selection
    // ---------------------------------------------------------------------

    /**
     * @brief Create a window surface using GLFW.
     *
     * GLFW provides a helper to create a VkSurfaceKHR for the native window.
     */
    void createSurface()
    {
        VkSurfaceKHR _surface;
        if (glfwCreateWindowSurface(*instance, window, nullptr, &_surface) != 0)
        {
            throw std::runtime_error("failed to create window surface!");
        }
        surface = vk::raii::SurfaceKHR(instance, _surface);
    }

    /**
     * @brief Check whether a physical device is suitable for our needs.
     *
     * Requirements:
     * - Vulkan 1.3 support
     * - a queue family that supports graphics
     * - required device extensions (swapchain)
     * - required features (shaderDrawParameters, dynamicRendering, extendedDynamicState)
     */
    bool isDeviceSuitable(vk::raii::PhysicalDevice const &pd)
    {
        // Check if the physicalDevice supports the Vulkan 1.3 API version
        bool supportsVulkan1_3 = pd.getProperties().apiVersion >= VK_API_VERSION_1_3;

        // Check if any of the queue families support graphics operations
        auto queueFamilies    = pd.getQueueFamilyProperties();
        bool supportsGraphics = false;
        for (auto const &qfp : queueFamilies)
        {
            if (qfp.queueFlags & vk::QueueFlagBits::eGraphics)
            {
                supportsGraphics = true;
                break;
            }
        }

        // Check if all required physicalDevice extensions are available
        auto availableDeviceExtensions = pd.enumerateDeviceExtensionProperties();
        bool supportsAllRequiredExtensions = true;
        for (auto const *reqExt : requiredDeviceExtension)
        {
            bool found = false;
            for (auto const &availExt : availableDeviceExtensions)
            {
                if (std::strcmp(availExt.extensionName, reqExt) == 0)
                {
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                supportsAllRequiredExtensions = false;
                break;
            }
        }

        // Check if the physicalDevice supports the required features
        vk::PhysicalDeviceFeatures2 baseFeatures{};
        vk::PhysicalDeviceVulkan11Features f11{};
        vk::PhysicalDeviceVulkan13Features f13{};
        vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT fExt{};

        baseFeatures.pNext = &f11;
        f11.pNext          = &f13;
        f13.pNext          = &fExt;

        auto features = pd.getFeatures2<
            vk::PhysicalDeviceFeatures2,
            vk::PhysicalDeviceVulkan11Features,
            vk::PhysicalDeviceVulkan13Features,
            vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
        >();

        bool supportsRequiredFeatures =
            features.get<vk::PhysicalDeviceVulkan11Features>().shaderDrawParameters &&
            features.get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
            features.get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState;


        // Return true if the physicalDevice meets all the criteria
        return supportsVulkan1_3 && supportsGraphics && supportsAllRequiredExtensions && supportsRequiredFeatures;
    }

    /**
     * @brief Pick a suitable physical device (GPU).
     *
     * Enumerates available physical devices and selects the first one that
     * satisfies isDeviceSuitable.
     */
    void pickPhysicalDevice()
    {
        std::vector<vk::raii::PhysicalDevice> physicalDevices = instance.enumeratePhysicalDevices();
        for (auto const &pd : physicalDevices)
        {
            if (isDeviceSuitable(pd))
            {
                physicalDevice = pd;
                return;
            }
        }
        throw std::runtime_error("failed to find a suitable GPU!");
    }

    // ---------------------------------------------------------------------
    // Logical device and queues
    // ---------------------------------------------------------------------

    /**
     * @brief Create the logical device and retrieve the graphics/present queue.
     *
     * Enables required Vulkan features via a feature chain and creates a
     * single queue that supports both graphics and presentation.
     */
    void createLogicalDevice()
    {
        std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();

        // get the first index into queueFamilyProperties which supports both graphics and present
        uint32_t queueIndex = ~0u;
        for (uint32_t qfpIndex = 0; qfpIndex < queueFamilyProperties.size(); qfpIndex++)
        {
            if ((queueFamilyProperties[qfpIndex].queueFlags & vk::QueueFlagBits::eGraphics) &&
                physicalDevice.getSurfaceSupportKHR(qfpIndex, *surface))
            {
                // found a queue family that supports both graphics and present
                queueIndex = qfpIndex;
                break;
            }
        }
        if (queueIndex == ~0u)
        {
            throw std::runtime_error("Could not find a queue for graphics and present -> terminating");
        }

        // query for Vulkan 1.3 features using a manual feature chain
        vk::PhysicalDeviceFeatures2 baseFeatures{};
        vk::PhysicalDeviceVulkan11Features f11{};
        vk::PhysicalDeviceVulkan13Features f13{};
        vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT fExt{};

        baseFeatures.pNext = &f11;
        f11.pNext          = &f13;
        f13.pNext          = &fExt;

        f11.shaderDrawParameters  = VK_TRUE;
        f13.dynamicRendering      = VK_TRUE;
        fExt.extendedDynamicState = VK_TRUE;

        // create a Device
        float                     queuePriority = 0.5f;
        vk::DeviceQueueCreateInfo deviceQueueCreateInfo{};
        deviceQueueCreateInfo.queueFamilyIndex = queueIndex;
        deviceQueueCreateInfo.queueCount       = 1;
        deviceQueueCreateInfo.pQueuePriorities = &queuePriority;

        vk::DeviceCreateInfo deviceCreateInfo{};
        deviceCreateInfo.pNext                   = &baseFeatures;
        deviceCreateInfo.queueCreateInfoCount    = 1;
        deviceCreateInfo.pQueueCreateInfos       = &deviceQueueCreateInfo;
        deviceCreateInfo.enabledExtensionCount   = static_cast<uint32_t>(requiredDeviceExtension.size());
        deviceCreateInfo.ppEnabledExtensionNames = requiredDeviceExtension.data();

        device = vk::raii::Device(physicalDevice, deviceCreateInfo);
        queue  = vk::raii::Queue(device, queueIndex, 0);
    }

    // ---------------------------------------------------------------------
    // Swapchain and image views
    // ---------------------------------------------------------------------

    /**
     * @brief Create the swap chain and retrieve its images.
     *
     * Chooses a surface format, present mode and extent, then creates the
     * swapchain and stores the images for later use.
     */
    void createSwapChain()
    {
        vk::SurfaceCapabilitiesKHR surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(*surface);
        swapChainExtent                                = chooseSwapExtent(surfaceCapabilities);
        uint32_t minImageCount                         = chooseSwapMinImageCount(surfaceCapabilities);

        std::vector<vk::SurfaceFormatKHR> availableFormats = physicalDevice.getSurfaceFormatsKHR(*surface);
        swapChainSurfaceFormat                             = chooseSwapSurfaceFormat(availableFormats);

        std::vector<vk::PresentModeKHR> availablePresentModes = physicalDevice.getSurfacePresentModesKHR(*surface);
        vk::PresentModeKHR              presentMode           = chooseSwapPresentMode(availablePresentModes);

        vk::SwapchainCreateInfoKHR swapChainCreateInfo{};
        swapChainCreateInfo.surface          = *surface;
        swapChainCreateInfo.minImageCount    = minImageCount;
        swapChainCreateInfo.imageFormat      = swapChainSurfaceFormat.format;
        swapChainCreateInfo.imageColorSpace  = swapChainSurfaceFormat.colorSpace;
        swapChainCreateInfo.imageExtent      = swapChainExtent;
        swapChainCreateInfo.imageArrayLayers = 1;
        swapChainCreateInfo.imageUsage       = vk::ImageUsageFlagBits::eColorAttachment;
        swapChainCreateInfo.imageSharingMode = vk::SharingMode::eExclusive;
        swapChainCreateInfo.preTransform     = surfaceCapabilities.currentTransform;
        swapChainCreateInfo.compositeAlpha   = vk::CompositeAlphaFlagBitsKHR::eOpaque;
        swapChainCreateInfo.presentMode      = presentMode;
        swapChainCreateInfo.clipped          = true;

        swapChain       = vk::raii::SwapchainKHR(device, swapChainCreateInfo);
        swapChainImages = swapChain.getImages();
    }

    /**
     * @brief Create image views for each swapchain image.
     *
     * Image views are required to use images as attachments in framebuffers.
     */
    void createImageViews()
    {
        assert(swapChainImageViews.empty());

        vk::ImageViewCreateInfo imageViewCreateInfo{};
        imageViewCreateInfo.viewType = vk::ImageViewType::e2D;
        imageViewCreateInfo.format   = swapChainSurfaceFormat.format;
        imageViewCreateInfo.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
        for (auto &image : swapChainImages)
        {
            imageViewCreateInfo.image = image;
            swapChainImageViews.emplace_back(device, imageViewCreateInfo);
        }
    }

    // ---------------------------------------------------------------------
    // Graphics pipeline (shaders + stages)
    // ---------------------------------------------------------------------

    /**
     * @brief Create the graphics pipeline (shader loading + stage setup).
     *
     * This function currently performs:
     *  - Loading SPIR-V bytecode produced by Slang (shaders/slang.spv)
     *  - Creating a vk::raii::ShaderModule from the SPIR-V
     *  - Creating vk::PipelineShaderStageCreateInfo for vertex and fragment
     *
     * The remainder of the pipeline (vertex input, input assembly, viewport,
     * rasterizer, multisampling, color blending, pipeline layout and pipeline
     * creation) will be added in subsequent steps. This function is intentionally
     * documented in detail because pipeline creation is the most complex part.
     *
     * Detailed notes:
     * - Slang compiles both vertex and fragment entry points into a single
     *   SPIR-V file in this tutorial (vertMain and fragMain).
     * - vk::raii::ShaderModule is RAII-managed and will be destroyed when it
     *   goes out of scope; pipeline creation copies the necessary data.
     */
    void createGraphicsPipeline()
    {
        // -------------------------
        // 1) Load SPIR-V from disk
        // -------------------------
        // The SPIR-V file should be produced by running slangc on your .slang
        // source. Example:
        //   slangc shaders/shader.slang -target spirv -emit-spirv-directly -entry vertMain -entry fragMain -o shaders/slang.spv
        auto shaderCode = readFile("shaders/slang.spv");

        // -------------------------
        // 2) Create shader module
        // -------------------------
        // Wrap the SPIR-V in a Vulkan shader module using RAII.
        vk::raii::ShaderModule shaderModule = createShaderModule(shaderCode);

        // -------------------------
        // 3) Create shader stage infos
        // -------------------------
        // Vertex stage
        vk::PipelineShaderStageCreateInfo vertShaderStageInfo{};
        vertShaderStageInfo.stage  = vk::ShaderStageFlagBits::eVertex;
        vertShaderStageInfo.module = *shaderModule;
        vertShaderStageInfo.pName  = "vertMain";

        // Fragment stage
        vk::PipelineShaderStageCreateInfo fragShaderStageInfo{};
        fragShaderStageInfo.stage  = vk::ShaderStageFlagBits::eFragment;
        fragShaderStageInfo.module = *shaderModule;
        fragShaderStageInfo.pName  = "fragMain";

        // Combine stages into an array for pipeline creation
        vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

        // -----------------------------------------------------------------
        // Next steps (to be implemented in later tutorial sections):
        // - vk::PipelineVertexInputStateCreateInfo
        // - vk::PipelineInputAssemblyStateCreateInfo
        // - vk::PipelineViewportStateCreateInfo
        // - vk::PipelineRasterizationStateCreateInfo
        // - vk::PipelineMultisampleStateCreateInfo
        // - vk::PipelineColorBlendStateCreateInfo
        // - vk::PipelineLayout (descriptor sets, push constants)
        // - vk::raii::Pipeline (graphics pipeline creation)
        // -----------------------------------------------------------------
        (void)shaderStages; // silence unused variable for now
    }

    /**
     * @brief Create a Vulkan shader module from SPIR-V bytecode.
     *
     * @param code Raw bytes of a SPIR-V file.
     * @return vk::raii::ShaderModule RAII wrapper around VkShaderModule.
     *
     * Note: The SPIR-V data must be 4-byte aligned. std::vector<char> is used
     * here for simplicity; ensure the file was written as binary.
     */
    [[nodiscard]] vk::raii::ShaderModule createShaderModule(const std::vector<char> &code) const
    {
        vk::ShaderModuleCreateInfo createInfo{};
        createInfo.codeSize = code.size();
        createInfo.pCode    = reinterpret_cast<const uint32_t *>(code.data());
        vk::raii::ShaderModule shaderModule{device, createInfo};

        return shaderModule;
    }

    // ---------------------------------------------------------------------
    // Swapchain helpers
    // ---------------------------------------------------------------------

    /**
     * @brief Choose the minimum number of images for the swapchain.
     *
     * Requests at least 3 images for triple buffering, clamped to the device's
     * maximum if one is specified.
     */
    static uint32_t chooseSwapMinImageCount(vk::SurfaceCapabilitiesKHR const &surfaceCapabilities)
    {
        auto minImageCount = std::max(3u, surfaceCapabilities.minImageCount);
        if ((0 < surfaceCapabilities.maxImageCount) && (surfaceCapabilities.maxImageCount < minImageCount))
        {
            minImageCount = surfaceCapabilities.maxImageCount;
        }
        return minImageCount;
    }

    /**
     * @brief Choose a surface format for the swapchain.
     *
     * Prefers B8G8R8A8_SRGB with SRGB nonlinear color space if available.
     */
    static vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR> &availableFormats)
    {
        assert(!availableFormats.empty());
        for (auto const &format : availableFormats)
        {
            if (format.format == vk::Format::eB8G8R8A8Srgb &&
                format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
            {
                return format;
            }
        }
        return availableFormats[0];
    }

    /**
     * @brief Choose a present mode for the swapchain.
     *
     * Prefers MAILBOX if available, otherwise falls back to FIFO (guaranteed).
     */
    static vk::PresentModeKHR chooseSwapPresentMode(std::vector<vk::PresentModeKHR> const &availablePresentModes)
    {
        bool hasMailbox = false;
        bool hasFifo    = false;
        for (auto pm : availablePresentModes)
        {
            if (pm == vk::PresentModeKHR::eMailbox)
                hasMailbox = true;
            if (pm == vk::PresentModeKHR::eFifo)
                hasFifo = true;
        }
        assert(hasFifo);
        return hasMailbox ? vk::PresentModeKHR::eMailbox : vk::PresentModeKHR::eFifo;
    }

    /**
     * @brief Choose the swap extent (resolution).
     *
     * If the surface has a fixed currentExtent, use it. Otherwise query the
     * framebuffer size from GLFW and clamp to allowed extents.
     */
    vk::Extent2D chooseSwapExtent(vk::SurfaceCapabilitiesKHR const &capabilities)
    {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        {
            return capabilities.currentExtent;
        }
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        return {
            std::clamp<uint32_t>(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
            std::clamp<uint32_t>(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)};
    }

    /**
     * @brief Get required instance extensions (GLFW + debug if enabled).
     */
    std::vector<const char *> getRequiredInstanceExtensions()
    {
        uint32_t glfwExtensionCount = 0;
        auto     glfwExtensions     = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector<const char *> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
        if (enableValidationLayers)
        {
            extensions.push_back(vk::EXTDebugUtilsExtensionName);
        }

        return extensions;
    }

    // ---------------------------------------------------------------------
    // File IO helper
    // ---------------------------------------------------------------------

    /**
     * @brief Reads an entire file into a byte buffer.
     *
     * @param filename Path to the file to read.
     * @return std::vector<char> Buffer containing the file contents.
     *
     * Used here to load SPIR-V shader bytecode from disk.
     */
    static std::vector<char> readFile(const std::string &filename)
    {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);
        if (!file.is_open())
        {
            throw std::runtime_error("failed to open file!");
        }
        std::vector<char> buffer(static_cast<size_t>(file.tellg()));
        file.seekg(0, std::ios::beg);
        file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        file.close();
        return buffer;
    }
};

int main()
{
    try
    {
        HelloTriangleApplication app;
        app.run();
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
