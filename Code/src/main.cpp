#include <vulkan/vulkan_core.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <vector>
#include <stdexcept>
#include <cstdlib>
#include <cstring>
#include <map>
#include <optional>

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

// List of validation layers we want to enable (debugging helpers for Vulkan)
const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

#ifdef NDEBUG
const bool enableValidationLayers = false; // Disable validation in release builds
#else
const bool enableValidationLayers = true;  // Enable validation in debug builds
#endif

// Helper functions to create/destroy Vulkan debug messenger
VkResult CreateDebugUtilsMessengerEXT(VkInstance instance,
                                      const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
                                      const VkAllocationCallbacks* pAllocator,
                                      VkDebugUtilsMessengerEXT* pDebugMessenger) {
    // vkGetInstanceProcAddr returns the address of a Vulkan function (here, the debug messenger creator)
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance,
                                   VkDebugUtilsMessengerEXT debugMessenger,
                                   const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;

    bool isComplete() {
        return graphicsFamily.has_value();
    }
};

class HelloTriangleApplication {
public:
    // Controls the lifetime of the application
    void run() {
        initWindow();     // Setup GLFW window
        initVulkan();     // Initialize Vulkan (instance + debug messenger)
        mainLoop();       // Main loop (just polling events for now)
        cleanup();        // Cleanup Vulkan and GLFW resources
    }

private:
    GLFWwindow* window;

    /* Vulkan instance represents the connection between
       the application and the Vulkan driver/runtime */
    VkInstance instance;

    // Debug messenger for reporting Vulkan validation layer warnings/errors
    VkDebugUtilsMessengerEXT debugMessenger;

    // Handle to the GPU we will eventually choose
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

    // Handle to Logical Device
    VkDevice device;

    // Handle to the Graphics queue
    VkQueue graphicsQueue;

    void initWindow() {
        glfwInit();

        /* Prevent GLFW from creating an OpenGL context,
           we use Vulkan for rendering instead */
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        window = glfwCreateWindow(WIDTH, HEIGHT, "TheSpookiestAmdusWindow", nullptr, nullptr);
    }

    void initVulkan() {
        createInstance();        // Create Vulkan instance
        setupDebugMessenger();   // Setup debug layer callbacks if validation layers are enabled
        pickPhysicalDevice();
        createLogDevice();    // Picks What hardware should be used for rendering
    }

    void mainLoop() {
        // Polls OS window/input events until the user closes the window
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
        }
    }

    void cleanup() {
        vkDestroyDevice(device, nullptr);

        if (enableValidationLayers) {
            DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
        }

        // Destroys the Vulkan instance and releases driver resources
        vkDestroyInstance(instance, nullptr);

        glfwDestroyWindow(window);

        glfwTerminate();
    }

    void createInstance() {
        // Make sure requested validation layers are available
        if (enableValidationLayers && !checkValidationLayerSupport()) {
            throw std::runtime_error("validation layers requested, but not available!");
        }

        /* Provides metadata about the application to the Vulkan driver,
           mostly used by debugging tools and validation layers */
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Chronica Regna Fractorum"; // Your custom app name
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        // Structure describing how the Vulkan instance should be created
        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        // Vulkan needs a list of extensions (like platform/window system integration)
        auto extensions = getRequiredExtensions();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        // Debug messenger info struct (optional, used only if validation layers enabled)
        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        if (enableValidationLayers) {
            // Tell Vulkan which validation layers to enable
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();

            /* Populate the debug messenger struct:
               This tells Vulkan which kinds of messages we care about (verbose, warnings, errors)
               and what function to call when a message occurs (debugCallback) */
            populateDebugMessengerCreateInfo(debugCreateInfo);

            // Attach debug messenger info to instance creation (pNext chain)
            createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
        } else {
            createInfo.enabledLayerCount = 0;
            createInfo.pNext = nullptr;
        }

        // Actually create the Vulkan instance
        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
            throw std::runtime_error("failed to create instance!");
        }
    }

    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
        createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;

        // Message severity flags: which types of messages to show
        createInfo.messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

        // Message type flags: general, validation, performance
        createInfo.messageType =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

        // Function to call whenever a message is triggered
        createInfo.pfnUserCallback = debugCallback;
    }

    void setupDebugMessenger() {
        if (!enableValidationLayers) return; // Only do this if debugging

        VkDebugUtilsMessengerCreateInfoEXT createInfo;
        populateDebugMessengerCreateInfo(createInfo);

        if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
            throw std::runtime_error("failed to set up debug messenger!");
        }
    }

    std::vector<const char*> getRequiredExtensions() {
        // GLFW tells us what extensions are needed to interface with the window system
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        // Convert GLFW array into a std::vector for convenience
        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        // If validation layers are enabled, we also need the debug utils extension
        if (enableValidationLayers) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        return extensions;
    }

    bool checkValidationLayerSupport() {
        // Query how many validation layers are available on the system
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        // Check that every requested validation layer exists
        for (const char* layerName : validationLayers) {
            bool layerFound = false;

            for (const auto& layerProperties : availableLayers) {
                if (strcmp(layerName, layerProperties.layerName) == 0) {
                    layerFound = true;
                    break;
                }
            }

            if (!layerFound) {
                return false; // requested layer not available
            }
        }

        return true;
    }

    void pickPhysicalDevice() {

        uint32_t deviceCount = 0;

        // First call: ask Vulkan how many GPUs support Vulkan
        // Passing nullptr means "just tell me the count"
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

        if (deviceCount == 0) {
            throw std::runtime_error("failed to find GPUs with Vulkan support!");
        }

        // Allocate a vector large enough to hold all GPU handles
        std::vector<VkPhysicalDevice> devices(deviceCount);

        // Second call: actually retrieve the GPU handles
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        /* multimap automatically sorts elements by key.
        Here the key is an integer score and the value is the GPU.

        This allows us to easily select the highest scoring GPU later. */
        std::multimap<int, VkPhysicalDevice> candidates;

        for (const auto& device : devices) {

            // Rate each GPU based on performance/features
            int score = rateDeviceSuitability(device);

            // Insert score + device pair into sorted container
            candidates.insert(std::make_pair(score, device));
        }

        /* rbegin() gives the LAST element in the sorted map,
        which corresponds to the highest score. */
        if (candidates.rbegin()->first > 0) {

            // Store the best GPU handle
            physicalDevice = candidates.rbegin()->second;

        } else {

            // If the best score is 0, no GPU met the minimum requirements
            throw std::runtime_error("failed to find a suitable GPU!");
        }
    }

    void createLogDevice() {
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = indices.graphicsFamily.value();
        queueCreateInfo.queueCount = 1;

        float queuePriority = 1.0f;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        VkPhysicalDeviceFeatures deviceFeatures{};

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

        createInfo.pQueueCreateInfos = &queueCreateInfo;
        createInfo.queueCreateInfoCount = 1;

        createInfo.pEnabledFeatures = &deviceFeatures;

        createInfo.enabledExtensionCount = 0;

        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        } else {
            createInfo.enabledLayerCount = 0;
        }

        if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
            throw std::runtime_error("failed to create logical device!");
        }

        vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
    }

    int rateDeviceSuitability(VkPhysicalDevice device) {
        int score = 0;
        QueueFamilyIndices indices = findQueueFamilies(device);

        VkPhysicalDeviceProperties deviceProperties;
        VkPhysicalDeviceFeatures deviceFeatures;

        // Query information about the GPU hardware
        vkGetPhysicalDeviceProperties(device, &deviceProperties);

        // Query which optional GPU features are supported
        vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

        // Discrete GPUs (real graphics cards) get a large bonus
        if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            score += 1000;
        }

        /* Maximum texture size supported by the GPU.
        Larger limits allow higher quality textures,
        so we add this value to the score. */
        score += deviceProperties.limits.maxImageDimension2D;

        // If the GPU does not support geometry shaders,
        // our application cannot run on it.
        if (!deviceFeatures.geometryShader) {
            return 0;
        }

        return score;
    }

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) {
        QueueFamilyIndices indices;
        uint32_t queueFamilyCount = 0;

        // Ask Vulkan how many queue families this GPU supports
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        // Allocate space to store the queue family descriptions
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);

        // Retrieve the actual queue family properties
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        int i = 0;

        for (const auto& queueFamily : queueFamilies) {
            /* queueFlags is a bitmask describing what this queue can do.
            VK_QUEUE_GRAPHICS_BIT means the queue can execute graphics commands. */
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.graphicsFamily = i;
            }
            // If all required queue families have been found, stop searching
            if (indices.isComplete()) {
                break;
            }

            i++;
        }

        return indices;
    }

    // Called by Vulkan to report validation layer messages
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData) {

        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
        return VK_FALSE; // returning VK_FALSE means we don't abort the Vulkan call
    }
};

int main() {
    HelloTriangleApplication app;

    try {
        app.run();
    } catch (const std::exception& e) {
        // Print any error thrown during initialization
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}