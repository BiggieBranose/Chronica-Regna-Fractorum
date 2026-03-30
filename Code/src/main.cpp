/**
 * @file main.cpp
 * @author Johannes Ravnevand Paulsen (Johannesravnpaulsen@gmail.com)
 * @brief For now its the entire engine and program, this is how the vulkan tutorial did it
 * @version 0.1.3
 * @date 2026-03-28
 * @copyright Copyright (c) 2026
 */

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <limits>
#include <optional>
#include <set>

/** @brief Window size constants. uint32_t is a 32-bit unsigned integer */
const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

/**
 * @brief Vector for the list of validation layers that vulkan will use
 *
 * Vulkan uses validation layers to check for errors and incorrect API usage.
 * "VK_LAYER_KHRONOS_validation" is the main debugging layer.
 */
const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

/**
 * @brief Required device extensions (swapchain)
 *
 * The swapchain extension is required to present images to the surface.
 */
const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

/** @brief Boolean that controls if the validation layers should be active or not depending on if you are in debug or release build */
#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

/**
 * @brief Create a Debug Utils Messenger EXT object
 *
 * Helper that loads the extension function pointer for creating the debug messenger.
 * @param instance The Vulkan instance/connection to the window
 * @param pCreateInfo Pointer to a struct that contains all the settings for the debug messenger
 * @param pAllocator Optional pointer to custom Vulkan memory allocator, almost always "nullptr"
 * @param pDebugMessenger Pointer to the handle that will store the created debug messenger
 * @return VkResult Returns VK_SUCCESS on success or an error code if the extension is unavailable
 */
VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

/** @brief Destroys the debug messenger */
void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

/**
 * @brief Struct that stores Queue families
 *
 * A queue family is a group of GPU queues that can perform certain types of work.
 * Some can draw graphics, some can present images to the screen, some can transfer data, some can compute.
 * This one stores "graphicsFamily" which supports graphics commands and "presentFamily" which does commands for presenting images to the window.
 * They use optional because when you search for the queue families you don't know if the GPU supports it so you make it so that the variable can be empty.
 * This also allows you to check if the variables are not empty later.
 * This is exactly what happens in "isComplete".
 */
struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

/**
 * @brief Stores swapchain support details for a physical device and surface
 *
 * Contains capabilities, available surface formats and present modes.
 */
struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

/**
 * @brief The Class for the entire program
 *
 * This class contains all state and functions for initializing GLFW, Vulkan,
 * running the main loop, and cleaning up resources.
 */
class CRF {
public:
    /**
     * @brief Main application loop
     *
     * Initializes window, then Vulkan, then starts the runtime and cleans up everything once it ends.
     */
    void run() {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:
    /** @brief Pointer to the GLFW window and the connection to the OS window system */
    GLFWwindow* window;

    /** @brief Represents the connection to the Vulkan library */
    VkInstance instance;
    /** @brief What receives the validation layer messages */
    VkDebugUtilsMessengerEXT debugMessenger;
    /** @brief Surface Vulkan will render to and it connects to the OS window */
    VkSurfaceKHR surface;

    /** @brief The chosen GPU to use. Starts as "VK_NULL_HANDLE" until one is picked */
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    /** @brief Created out of the physical device. The interface to the GPU */
    VkDevice device;

    /** @brief Queue that can execute graphics commands */
    VkQueue graphicsQueue;
    /** @brief Queue that presents images to the window */
    VkQueue presentQueue;

    /** @brief The swapchain handle used for presenting images */
    VkSwapchainKHR swapChain;
    /** @brief Images owned by the swapchain */
    std::vector<VkImage> swapChainImages;
    /** @brief Format of the swapchain images */
    VkFormat swapChainImageFormat;
    /** @brief Extent (width/height) of the swapchain images */
    VkExtent2D swapChainExtent;

    /**
     * @brief Creates a non‑resizable GLFW window with Vulkan support
     *
     * Sets GLFW to avoid creating an OpenGL context since Vulkan manages rendering itself and also makes the window non-resizable since that would mean I had to make swapchain reconstruction.
     */
    void initWindow() {
        // Initialize the GLFW library
        glfwInit();

        // Tell GLFW not to create an OpenGL context (we use Vulkan)
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        // Make the window non-resizable to avoid handling swapchain recreation here
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        // Create the GLFW window
        window = glfwCreateWindow(WIDTH, HEIGHT, "TheSpookiestAmdusWindow", nullptr, nullptr);
    }

    /**
     * @brief Initializes Vulkan setting up everything it will need before entering the main loop
     *
     * Creating the instance, debug messenger, surface, selecting a physical device, creating the logical device, and creating the swapchain.
     */
    void initVulkan() {
        createInstance();
        setupDebugMessenger();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapChain();
    }

    /** @brief Runs all events until glfwWindowShouldClose = true */
    void mainLoop() {
        // Poll events continuously until the window should close
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
        }
    }

    /**
     * @brief Cleans up everything when the program ends
     *
     * Destroys Vulkan objects in the correct order and terminates GLFW.
     */
    void cleanup() {
        // Destroy swapchain first
        vkDestroySwapchainKHR(device, swapChain, nullptr);
        // Destroy logical device
        vkDestroyDevice(device, nullptr);

        // Destroy debug messenger if validation layers were enabled
        if (enableValidationLayers) {
            DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
        }

        // Destroy the surface and instance
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);

        // Destroy the GLFW window and terminate GLFW
        glfwDestroyWindow(window);
        glfwTerminate();
    }

    /**
     * @brief Boot sequence, it sets up everything application related and makes the instance
     *
     * Checks validation layer availability, fills application info, requests required extensions and layers,
     * optionally attaches debug messenger info to the pNext chain, and creates the Vulkan instance.
     */
    void createInstance() {
        // Make sure you don't enable validation layers that your GPU doesn't support
        if (enableValidationLayers && !checkValidationLayerSupport()) {
            throw std::runtime_error("validation layers requested, but not available!");
        }

        // Optional but recommended. Tells Vulkan app name, engine name, what Vulkan version you want and other things about version numbers
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "CRF";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "BranoseEngine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        // Tells Vulkan what extensions, validation layers and optional debug messenger to use
        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        // Adds the required extensions returned by GLFW (Wayland/X11/Win32 depending on platform)
        auto extensions = getRequiredExtensions();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        // Adds the validation layers and debug messenger (if enabled)
        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();

            populateDebugMessengerCreateInfo(debugCreateInfo);
            // Attaching the debug messenger info to "createInfo.pNext", so Vulkan uses the debug callback during instance creation itself.
            createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;
        } else {
            createInfo.enabledLayerCount = 0;
            createInfo.pNext = nullptr;
        }

        // Creates the actual Vulkan instance
        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
            throw std::runtime_error("failed to create instance!");
        }
    }

    /**
     * @brief Configures the debug messenger
     *
     * Fills the VkDebugUtilsMessengerCreateInfoEXT structure with desired message severities and types,
     * and sets the callback function.
     */
    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
        createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = debugCallback;
    }

    /**
     * @brief Applies the configuration and makes the debug messenger (if enabled)
     *
     * Calls the extension loader helper to create the debug messenger.
     */
    void setupDebugMessenger() {
        if (!enableValidationLayers) return;

        VkDebugUtilsMessengerCreateInfoEXT createInfo;
        populateDebugMessengerCreateInfo(createInfo);

        if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
            throw std::runtime_error("failed to set up debug messenger!");
        }
    }

    /**
     * @brief Creates a Vulkan surface that connects your GLFW window to Vulkan
     *
     * Uses GLFW to create a platform-specific surface compatible with the current window.
     */
    void createSurface() {
        if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
            throw std::runtime_error("failed to create window surface!");
        }
    }

    /**
     * @brief Selects a suitable physical device (GPU)
     *
     * Enumerates physical devices and picks the first one that is suitable according to isDeviceSuitable.
     */
    void pickPhysicalDevice() {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

        if (deviceCount == 0) {
            throw std::runtime_error("failed to find GPUs with Vulkan support!");
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        for (const auto& device : devices) {
            if (isDeviceSuitable(device)) {
                physicalDevice = device;
                break;
            }
        }

        if (physicalDevice == VK_NULL_HANDLE) {
            throw std::runtime_error("failed to find a suitable GPU!");
        }
    }

    /**
     * @brief Creates the logical device and retrieves queue handles
     *
     * Builds VkDeviceCreateInfo with required queue create infos, enabled features, and device extensions,
     * then creates the logical device and fetches the graphics and present queues.
     */
    void createLogicalDevice() {
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};

        float queuePriority = 1.0f;
        for (uint32_t queueFamily : uniqueQueueFamilies) {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        // No special device features required for this tutorial
        VkPhysicalDeviceFeatures deviceFeatures{};

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();

        createInfo.pEnabledFeatures = &deviceFeatures;

        // Enable required device extensions (swapchain)
        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();

        // Enable validation layers on the device if requested (deprecated on modern Vulkan but kept for compatibility)
        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        } else {
            createInfo.enabledLayerCount = 0;
        }

        // Create the logical device
        if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
            throw std::runtime_error("failed to create logical device!");
        }

        // Retrieve queue handles for graphics and present
        vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
        vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
    }

    /**
     * @brief Creates the swapchain and retrieves its images
     *
     * Queries swapchain support, chooses format/present mode/extent, creates the swapchain,
     * and fetches the swapchain images.
     */
    void createSwapChain() {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
        VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
        VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

        // Request one more than the minimum to avoid waiting on the driver
        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
        if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
            imageCount = swapChainSupport.capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = surface;

        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        // Images will be used as color attachments
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
        uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

        // If graphics and present are different queue families, use concurrent sharing mode
        if (indices.graphicsFamily != indices.presentFamily) {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        } else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }

        // No transform
        createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
        // Ignore alpha blending with other windows
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode;
        // We don't care about pixels obscured by other windows
        createInfo.clipped = VK_TRUE;

        createInfo.oldSwapchain = VK_NULL_HANDLE;

        // Create the swapchain
        if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
            throw std::runtime_error("failed to create swap chain!");
        }

        // Retrieve swapchain images
        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
        swapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

        swapChainImageFormat = surfaceFormat.format;
        swapChainExtent = extent;
    }

    /**
     * @brief Chooses the best available surface format
     *
     * Prefers VK_FORMAT_B8G8R8A8_SRGB with SRGB nonlinear color space, otherwise returns the first available.
     */
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
        for (const auto& availableFormat : availableFormats) {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
                availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return availableFormat;
            }
        }
        return availableFormats[0];
    }

    /**
     * @brief Chooses the present mode for the swapchain
     *
     * Prefers MAILBOX for low latency if available, otherwise falls back to FIFO which is guaranteed to be available.
     */
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
        for (const auto& availablePresentMode : availablePresentModes) {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                return availablePresentMode;
            }
        }
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    /**
     * @brief Chooses the swapchain extent (resolution)
     *
     * If the surface specifies a fixed size, use it. Otherwise query the GLFW framebuffer size and clamp to allowed extents.
     */
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            // The surface already defined the size (e.g., on some platforms)
            return capabilities.currentExtent;
        } else {
            int width, height;
            glfwGetFramebufferSize(window, &width, &height);

            VkExtent2D actualExtent = {
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height)
            };

            actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

            return actualExtent;
        }
    }

    /**
     * @brief Queries swapchain support details for a given physical device and the current surface
     *
     * Fills capabilities, available formats and present modes.
     */
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) {
        SwapChainSupportDetails details;

        // Get surface capabilities (min/max images, current extent, transforms)
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

        // Get supported surface formats
        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
        if (formatCount != 0) {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
        }

        // Get supported present modes
        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
        if (presentModeCount != 0) {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
        }

        return details;
    }

    /**
     * @brief Checks whether a physical device is suitable for our needs
     *
     * Verifies queue family support, required device extensions, and swapchain adequacy.
     */
    bool isDeviceSuitable(VkPhysicalDevice device) {
        QueueFamilyIndices indices = findQueueFamilies(device);

        bool extensionsSupported = checkDeviceExtensionSupport(device);

        bool swapChainAdequate = false;
        if (extensionsSupported) {
            SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
            swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
        }

        return indices.isComplete() && extensionsSupported && swapChainAdequate;
    }

    /**
     * @brief Checks whether the physical device supports required device extensions
     *
     * Enumerates device extension properties and ensures all required extensions are present.
     */
    bool checkDeviceExtensionSupport(VkPhysicalDevice device) {
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

        for (const auto& extension : availableExtensions) {
            requiredExtensions.erase(extension.extensionName);
        }

        return requiredExtensions.empty();
    }

    /**
     * @brief Finds queue families that support graphics and presentation
     *
     * Iterates over queue families and checks for VK_QUEUE_GRAPHICS_BIT and surface present support.
     */
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) {
        QueueFamilyIndices indices;

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        int i = 0;
        for (const auto& queueFamily : queueFamilies) {
            // Check for graphics capability
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.graphicsFamily = i;
            }

            // Check for present support to the created surface
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

            if (presentSupport) {
                indices.presentFamily = i;
            }

            if (indices.isComplete()) {
                break;
            }

            i++;
        }

        return indices;
    }

    /**
     * @brief Retrieves the instance extensions required by GLFW and optionally adds debug utils
     *
     * Uses glfwGetRequiredInstanceExtensions to remain cross-platform (Wayland/X11/Win32).
     */
    std::vector<const char*> getRequiredExtensions() {
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        if (enableValidationLayers) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        return extensions;
    }

    /**
     * @brief Checks whether requested validation layers are available on the system
     *
     * Enumerates instance layer properties and verifies each requested layer name exists.
     */
    bool checkValidationLayerSupport() {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for (const char* layerName : validationLayers) {
            bool layerFound = false;

            for (const auto& layerProperties : availableLayers) {
                if (strcmp(layerName, layerProperties.layerName) == 0) {
                    layerFound = true;
                    break;
                }
            }

            if (!layerFound) {
                return false;
            }
        }

        return true;
    }

    /**
     * @brief Vulkan debug callback used by the validation layers
     *
     * Prints validation messages to stderr.
     */
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData) {

        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
        return VK_FALSE;
    }
};

int main() {
    CRF app;

    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
