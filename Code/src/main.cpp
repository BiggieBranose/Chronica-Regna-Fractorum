/**
 * @file main.cpp
 * @author Johannes Ravnevand Paulsen (Johannesravnpaulsen@gmail.com)
 * @brief Full Vulkan engine — single-file, follows the vulkan-tutorial.com structure.
 *        Covers: instance, validation layers, surface, device selection, swapchain,
 *        image views, render pass, descriptor set layout, graphics pipeline, framebuffers,
 *        command pool/buffers, MSAA color+depth resources, texture image + mipmaps,
 *        sampler, OBJ model loading, vertex/index/uniform buffers, descriptor pool/sets,
 *        synchronisation (semaphores + fences), per-frame draw loop, swapchain recreation.
 * @version 0.2.0
 * @date 2026-03-28
 * @copyright Copyright (c) 2026
 */

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <iostream>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <chrono>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <limits>
#include <array>
#include <optional>
#include <set>
#include <unordered_map>

/** @brief Window size constants. uint32_t is a 32-bit unsigned integer */
const uint32_t WIDTH  = 800;
const uint32_t HEIGHT = 600;

const std::string MODEL_PATH   = "models/viking_room.obj";
const std::string TEXTURE_PATH = "textures/viking_room.png";

/** @brief How many frames the CPU is allowed to prepare ahead of the GPU */
const int MAX_FRAMES_IN_FLIGHT = 2;

/**
 * @brief Validation layers Vulkan will use.
 *
 * "VK_LAYER_KHRONOS_validation" is the main debugging layer — it checks for
 * incorrect API usage, memory hazards, and synchronisation issues.
 */
const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

/**
 * @brief Required device extensions.
 *
 * The swapchain extension lets us present rendered images to a window surface.
 */
const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

/** @brief Validation layers are compiled out in release builds */
#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

// ---------------------------------------------------------------------------
// Extension function loaders
// ---------------------------------------------------------------------------

/**
 * @brief Loads and calls vkCreateDebugUtilsMessengerEXT.
 *
 * This function is an extension so its address must be looked up at runtime
 * via vkGetInstanceProcAddr.
 */
VkResult CreateDebugUtilsMessengerEXT(
    VkInstance                                instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks*              pAllocator,
    VkDebugUtilsMessengerEXT*                 pDebugMessenger)
{
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr)
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    return VK_ERROR_EXTENSION_NOT_PRESENT;
}

/** @brief Loads and calls vkDestroyDebugUtilsMessengerEXT */
void DestroyDebugUtilsMessengerEXT(
    VkInstance                   instance,
    VkDebugUtilsMessengerEXT     debugMessenger,
    const VkAllocationCallbacks* pAllocator)
{
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)
        vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr)
        func(instance, debugMessenger, pAllocator);
}

// ---------------------------------------------------------------------------
// Helper structs
// ---------------------------------------------------------------------------

/**
 * @brief Stores queue family indices for graphics and presentation.
 *
 * A queue family is a group of GPU queues that can perform certain types of work.
 * std::optional lets us detect "not yet found" vs. index 0.
 */
struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

/**
 * @brief Swapchain capabilities, formats and present modes for a device+surface pair.
 */
struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR        capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR>   presentModes;
};

/**
 * @brief A single vertex as sent to the GPU.
 *
 * Contains position, colour tint and texture coordinate.
 * The static helpers return the Vulkan binding/attribute descriptions so the
 * pipeline knows how to interpret the vertex buffer.
 */
struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texCoord;

    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription desc{};
        desc.binding   = 0;
        desc.stride    = sizeof(Vertex);
        desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return desc;
    }

    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 3> attrs{};

        attrs[0].binding  = 0;
        attrs[0].location = 0;
        attrs[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
        attrs[0].offset   = offsetof(Vertex, pos);

        attrs[1].binding  = 0;
        attrs[1].location = 1;
        attrs[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
        attrs[1].offset   = offsetof(Vertex, color);

        attrs[2].binding  = 0;
        attrs[2].location = 2;
        attrs[2].format   = VK_FORMAT_R32G32_SFLOAT;
        attrs[2].offset   = offsetof(Vertex, texCoord);

        return attrs;
    }

    bool operator==(const Vertex& other) const {
        return pos == other.pos && color == other.color && texCoord == other.texCoord;
    }
};

/** @brief Hash for Vertex so it can be used as an unordered_map key */
namespace std {
    template<> struct hash<Vertex> {
        size_t operator()(Vertex const& v) const {
            return ((hash<glm::vec3>()(v.pos) ^
                    (hash<glm::vec3>()(v.color) << 1)) >> 1) ^
                    (hash<glm::vec2>()(v.texCoord) << 1);
        }
    };
}

/**
 * @brief Uniform buffer data uploaded once per frame.
 *
 * alignas(16) satisfies Vulkan's std140 layout requirement for mat4.
 */
struct UniformBufferObject {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};

// ---------------------------------------------------------------------------
// Main application class
// ---------------------------------------------------------------------------

/**
 * @brief The class for the entire engine and program.
 *
 * Holds all Vulkan/GLFW state and every function needed to initialise,
 * run and tear down the application.
 */
class CRF {
public:
    /**
     * @brief Main entry point.
     *
     * Initialises the window, then Vulkan, runs the render loop,
     * and cleans up everything once the window is closed.
     */
    void run() {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:
    // --- GLFW ---
    /** @brief Pointer to the OS window */
    GLFWwindow* window;

    // --- Core Vulkan handles ---
    /** @brief Connection to the Vulkan library */
    VkInstance instance;
    /** @brief Receives validation layer messages */
    VkDebugUtilsMessengerEXT debugMessenger;
    /** @brief Platform surface Vulkan renders to */
    VkSurfaceKHR surface;

    /** @brief Chosen GPU. Stays VK_NULL_HANDLE until pickPhysicalDevice succeeds */
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    /** @brief Maximum MSAA sample count the GPU supports */
    VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;
    /** @brief Logical device — the software interface to the GPU */
    VkDevice device;

    // --- Queues ---
    VkQueue graphicsQueue;
    VkQueue presentQueue;

    // --- Swapchain ---
    VkSwapchainKHR            swapChain;
    std::vector<VkImage>      swapChainImages;
    VkFormat                  swapChainImageFormat;
    VkExtent2D                swapChainExtent;
    std::vector<VkImageView>  swapChainImageViews;
    std::vector<VkFramebuffer> swapChainFramebuffers;

    // --- Pipeline ---
    VkRenderPass          renderPass;
    VkDescriptorSetLayout descriptorSetLayout;
    VkPipelineLayout      pipelineLayout;
    VkPipeline            graphicsPipeline;

    // --- Commands ---
    VkCommandPool                commandPool;
    std::vector<VkCommandBuffer> commandBuffers;

    // --- MSAA colour resolve image ---
    VkImage        colorImage;
    VkDeviceMemory colorImageMemory;
    VkImageView    colorImageView;

    // --- Depth buffer ---
    VkImage        depthImage;
    VkDeviceMemory depthImageMemory;
    VkImageView    depthImageView;

    // --- Texture ---
    uint32_t       mipLevels;
    VkImage        textureImage;
    VkDeviceMemory textureImageMemory;
    VkImageView    textureImageView;
    VkSampler      textureSampler;

    // --- Geometry ---
    std::vector<Vertex>   vertices;
    std::vector<uint32_t> indices;
    VkBuffer       vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    VkBuffer       indexBuffer;
    VkDeviceMemory indexBufferMemory;

    // --- Uniform buffers (one per frame-in-flight) ---
    std::vector<VkBuffer>       uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;
    std::vector<void*>          uniformBuffersMapped;

    // --- Descriptors ---
    VkDescriptorPool             descriptorPool;
    std::vector<VkDescriptorSet> descriptorSets;

    // --- Per-frame sync objects ---
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence>     inFlightFences;
    uint32_t currentFrame = 0;

    /** @brief Set by the GLFW resize callback so drawFrame can recreate the swapchain */
    bool framebufferResized = false;

    // -----------------------------------------------------------------------
    // Initialisation
    // -----------------------------------------------------------------------

    /**
     * @brief Creates a GLFW window that supports Vulkan and swapchain resize.
     */
    void initWindow() {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // No OpenGL context
        window = glfwCreateWindow(WIDTH, HEIGHT, "TheSpookiestAmdusWindow", nullptr, nullptr);
        glfwSetWindowUserPointer(window, this);
        glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
    }

    /** @brief GLFW callback — marks the swapchain as out of date when the window is resized */
    static void framebufferResizeCallback(GLFWwindow* window, int /*width*/, int /*height*/) {
        auto app = reinterpret_cast<CRF*>(glfwGetWindowUserPointer(window));
        app->framebufferResized = true;
    }

    /**
     * @brief Initialises every Vulkan object in dependency order.
     */
    void initVulkan() {
        createInstance();
        setupDebugMessenger();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createImageViews();
        createRenderPass();
        createDescriptorSetLayout();
        createGraphicsPipeline();
        createCommandPool();
        createColorResources();
        createDepthResources();
        createFramebuffers();
        createTextureImage();
        createTextureImageView();
        createTextureSampler();
        loadModel();
        createVertexBuffer();
        createIndexBuffer();
        createUniformBuffers();
        createDescriptorPool();
        createDescriptorSets();
        createCommandBuffers();
        createSyncObjects();
    }

    /**
     * @brief Runs the render loop until the window is closed.
     *
     * vkDeviceWaitIdle at the end ensures the GPU is idle before cleanup begins.
     */
    void mainLoop() {
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            drawFrame();
        }
        vkDeviceWaitIdle(device);
    }

    // -----------------------------------------------------------------------
    // Cleanup
    // -----------------------------------------------------------------------

    /** @brief Destroys everything that depends on the swapchain resolution */
    void cleanupSwapChain() {
        vkDestroyImageView(device, depthImageView, nullptr);
        vkDestroyImage(device, depthImage, nullptr);
        vkFreeMemory(device, depthImageMemory, nullptr);

        vkDestroyImageView(device, colorImageView, nullptr);
        vkDestroyImage(device, colorImage, nullptr);
        vkFreeMemory(device, colorImageMemory, nullptr);

        for (auto fb : swapChainFramebuffers)
            vkDestroyFramebuffer(device, fb, nullptr);

        for (auto iv : swapChainImageViews)
            vkDestroyImageView(device, iv, nullptr);

        vkDestroySwapchainKHR(device, swapChain, nullptr);
    }

    /**
     * @brief Destroys all Vulkan objects and terminates GLFW.
     *
     * Objects are destroyed in reverse creation order so that no handle is
     * freed while something else still references it.
     */
    void cleanup() {
        cleanupSwapChain();

        vkDestroyPipeline(device, graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        vkDestroyRenderPass(device, renderPass, nullptr);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroyBuffer(device, uniformBuffers[i], nullptr);
            vkFreeMemory(device, uniformBuffersMemory[i], nullptr);
        }

        vkDestroyDescriptorPool(device, descriptorPool, nullptr);

        vkDestroySampler(device, textureSampler, nullptr);
        vkDestroyImageView(device, textureImageView, nullptr);
        vkDestroyImage(device, textureImage, nullptr);
        vkFreeMemory(device, textureImageMemory, nullptr);

        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

        vkDestroyBuffer(device, indexBuffer, nullptr);
        vkFreeMemory(device, indexBufferMemory, nullptr);

        vkDestroyBuffer(device, vertexBuffer, nullptr);
        vkFreeMemory(device, vertexBufferMemory, nullptr);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
            vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
            vkDestroyFence(device, inFlightFences[i], nullptr);
        }

        vkDestroyCommandPool(device, commandPool, nullptr);
        vkDestroyDevice(device, nullptr);

        if (enableValidationLayers)
            DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);

        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);

        glfwDestroyWindow(window);
        glfwTerminate();
    }

    // -----------------------------------------------------------------------
    // Swapchain recreation (window resize)
    // -----------------------------------------------------------------------

    /**
     * @brief Recreates all swapchain-dependent objects after a resize.
     *
     * Blocks while the window is minimised (width/height == 0).
     */
    void recreateSwapChain() {
        int width = 0, height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        while (width == 0 || height == 0) {
            glfwGetFramebufferSize(window, &width, &height);
            glfwWaitEvents();
        }

        vkDeviceWaitIdle(device);
        cleanupSwapChain();

        createSwapChain();
        createImageViews();
        createColorResources();
        createDepthResources();
        createFramebuffers();
    }

    // -----------------------------------------------------------------------
    // Instance
    // -----------------------------------------------------------------------

    /**
     * @brief Creates the Vulkan instance.
     *
     * Sets application/engine metadata, requests required extensions from GLFW,
     * optionally enables validation layers, and attaches an early debug messenger
     * via pNext so errors during instance creation itself are caught.
     */
    void createInstance() {
        if (enableValidationLayers && !checkValidationLayerSupport())
            throw std::runtime_error("validation layers requested, but not available!");

        VkApplicationInfo appInfo{};
        appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName   = "CRF";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName        = "BranoseEngine";
        appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion         = VK_API_VERSION_1_0;

        VkInstanceCreateInfo createInfo{};
        createInfo.sType            = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        auto extensions = getRequiredExtensions();
        createInfo.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        if (enableValidationLayers) {
            createInfo.enabledLayerCount   = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
            populateDebugMessengerCreateInfo(debugCreateInfo);
            createInfo.pNext = &debugCreateInfo;
        } else {
            createInfo.enabledLayerCount = 0;
            createInfo.pNext             = nullptr;
        }

        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
            throw std::runtime_error("failed to create instance!");
    }

    // -----------------------------------------------------------------------
    // Debug messenger
    // -----------------------------------------------------------------------

    /**
     * @brief Fills a VkDebugUtilsMessengerCreateInfoEXT with our desired settings.
     *
     * Captures verbose, warning and error severities for all message types.
     * This struct is reused both for the instance pNext chain and the real messenger.
     */
    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& ci) {
        ci = {};
        ci.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        ci.messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        ci.messageType =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT    |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        ci.pfnUserCallback = debugCallback;
    }

    /** @brief Creates the persistent debug messenger (validation layers must be enabled) */
    void setupDebugMessenger() {
        if (!enableValidationLayers) return;

        VkDebugUtilsMessengerCreateInfoEXT ci{};
        populateDebugMessengerCreateInfo(ci);

        if (CreateDebugUtilsMessengerEXT(instance, &ci, nullptr, &debugMessenger) != VK_SUCCESS)
            throw std::runtime_error("failed to set up debug messenger!");
    }

    // -----------------------------------------------------------------------
    // Surface
    // -----------------------------------------------------------------------

    /**
     * @brief Asks GLFW to create a platform-specific Vulkan surface for the window.
     *
     * GLFW handles the Wayland/XCB/Win32 difference automatically.
     */
    void createSurface() {
        if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
            throw std::runtime_error("failed to create window surface!");
    }

    // -----------------------------------------------------------------------
    // Physical device
    // -----------------------------------------------------------------------

    /**
     * @brief Enumerates all GPUs and picks the first suitable one.
     *
     * Also queries the maximum MSAA sample count supported by the chosen device.
     */
    void pickPhysicalDevice() {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

        if (deviceCount == 0)
            throw std::runtime_error("failed to find GPUs with Vulkan support!");

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        for (const auto& dev : devices) {
            if (isDeviceSuitable(dev)) {
                physicalDevice = dev;
                msaaSamples    = getMaxUsableSampleCount();
                break;
            }
        }

        if (physicalDevice == VK_NULL_HANDLE)
            throw std::runtime_error("failed to find a suitable GPU!");
    }

    /**
     * @brief Returns true if the device supports required queues, extensions, swapchain and anisotropy.
     */
    bool isDeviceSuitable(VkPhysicalDevice dev) {
        QueueFamilyIndices idx = findQueueFamilies(dev);

        bool extOk = checkDeviceExtensionSupport(dev);

        bool scOk = false;
        if (extOk) {
            SwapChainSupportDetails sc = querySwapChainSupport(dev);
            scOk = !sc.formats.empty() && !sc.presentModes.empty();
        }

        VkPhysicalDeviceFeatures feat{};
        vkGetPhysicalDeviceFeatures(dev, &feat);

        return idx.isComplete() && extOk && scOk && feat.samplerAnisotropy;
    }

    /**
     * @brief Verifies that every extension in deviceExtensions is available on the device.
     */
    bool checkDeviceExtensionSupport(VkPhysicalDevice dev) {
        uint32_t count = 0;
        vkEnumerateDeviceExtensionProperties(dev, nullptr, &count, nullptr);
        std::vector<VkExtensionProperties> available(count);
        vkEnumerateDeviceExtensionProperties(dev, nullptr, &count, available.data());

        std::set<std::string> required(deviceExtensions.begin(), deviceExtensions.end());
        for (const auto& ext : available)
            required.erase(ext.extensionName);

        return required.empty();
    }

    // -----------------------------------------------------------------------
    // Logical device
    // -----------------------------------------------------------------------

    /**
     * @brief Creates the logical device and retrieves graphics + present queue handles.
     *
     * Uses a std::set to deduplicate queue families — on most desktop GPUs graphics
     * and present share the same family, but they can differ (e.g. headless setups).
     */
    void createLogicalDevice() {
        QueueFamilyIndices idx = findQueueFamilies(physicalDevice);

        std::vector<VkDeviceQueueCreateInfo> queueCIs;
        std::set<uint32_t> uniqueFamilies = {
            idx.graphicsFamily.value(),
            idx.presentFamily.value()
        };

        float priority = 1.0f;
        for (uint32_t family : uniqueFamilies) {
            VkDeviceQueueCreateInfo qi{};
            qi.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            qi.queueFamilyIndex = family;
            qi.queueCount       = 1;
            qi.pQueuePriorities = &priority;
            queueCIs.push_back(qi);
        }

        VkPhysicalDeviceFeatures features{};
        features.samplerAnisotropy = VK_TRUE;

        VkDeviceCreateInfo ci{};
        ci.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        ci.queueCreateInfoCount    = static_cast<uint32_t>(queueCIs.size());
        ci.pQueueCreateInfos       = queueCIs.data();
        ci.pEnabledFeatures        = &features;
        ci.enabledExtensionCount   = static_cast<uint32_t>(deviceExtensions.size());
        ci.ppEnabledExtensionNames = deviceExtensions.data();

        // Deprecated on modern Vulkan but kept for older driver compatibility
        if (enableValidationLayers) {
            ci.enabledLayerCount   = static_cast<uint32_t>(validationLayers.size());
            ci.ppEnabledLayerNames = validationLayers.data();
        } else {
            ci.enabledLayerCount = 0;
        }

        if (vkCreateDevice(physicalDevice, &ci, nullptr, &device) != VK_SUCCESS)
            throw std::runtime_error("failed to create logical device!");

        vkGetDeviceQueue(device, idx.graphicsFamily.value(), 0, &graphicsQueue);
        vkGetDeviceQueue(device, idx.presentFamily.value(),  0, &presentQueue);
    }

    // -----------------------------------------------------------------------
    // Swapchain
    // -----------------------------------------------------------------------

    /**
     * @brief Creates the swapchain and retrieves its images.
     *
     * Chooses the best available surface format, present mode and resolution,
     * then requests minImageCount+1 images to avoid stalling on the driver.
     * If graphics and present queues differ, concurrent sharing is used.
     */
    void createSwapChain() {
        SwapChainSupportDetails support = querySwapChainSupport(physicalDevice);

        VkSurfaceFormatKHR fmt   = chooseSwapSurfaceFormat(support.formats);
        VkPresentModeKHR   mode  = chooseSwapPresentMode(support.presentModes);
        VkExtent2D         ext   = chooseSwapExtent(support.capabilities);

        uint32_t imageCount = support.capabilities.minImageCount + 1;
        if (support.capabilities.maxImageCount > 0 &&
            imageCount > support.capabilities.maxImageCount)
            imageCount = support.capabilities.maxImageCount;

        VkSwapchainCreateInfoKHR ci{};
        ci.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        ci.surface          = surface;
        ci.minImageCount    = imageCount;
        ci.imageFormat      = fmt.format;
        ci.imageColorSpace  = fmt.colorSpace;
        ci.imageExtent      = ext;
        ci.imageArrayLayers = 1;
        ci.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        QueueFamilyIndices idx = findQueueFamilies(physicalDevice);
        uint32_t queueIndices[] = {idx.graphicsFamily.value(), idx.presentFamily.value()};

        if (idx.graphicsFamily != idx.presentFamily) {
            ci.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
            ci.queueFamilyIndexCount = 2;
            ci.pQueueFamilyIndices   = queueIndices;
        } else {
            ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }

        ci.preTransform   = support.capabilities.currentTransform;
        ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        ci.presentMode    = mode;
        ci.clipped        = VK_TRUE;
        ci.oldSwapchain   = VK_NULL_HANDLE;

        if (vkCreateSwapchainKHR(device, &ci, nullptr, &swapChain) != VK_SUCCESS)
            throw std::runtime_error("failed to create swap chain!");

        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
        swapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

        swapChainImageFormat = fmt.format;
        swapChainExtent      = ext;
    }

    /**
     * @brief Prefers SRGB 32-bit BGRA. Falls back to whatever the driver offers first.
     */
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) {
        for (const auto& f : formats)
            if (f.format == VK_FORMAT_B8G8R8A8_SRGB &&
                f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                return f;
        return formats[0];
    }

    /**
     * @brief Prefers MAILBOX (triple-buffer) for low latency. Falls back to FIFO (vsync).
     *
     * FIFO is always guaranteed to be available by the Vulkan spec.
     */
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& modes) {
        for (const auto& m : modes)
            if (m == VK_PRESENT_MODE_MAILBOX_KHR)
                return m;
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    /**
     * @brief Chooses the swapchain resolution.
     *
     * Most platforms set currentExtent to the window size. On platforms that
     * set it to UINT32_MAX we query the actual framebuffer size and clamp it.
     */
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& caps) {
        if (caps.currentExtent.width != std::numeric_limits<uint32_t>::max())
            return caps.currentExtent;

        int w, h;
        glfwGetFramebufferSize(window, &w, &h);

        VkExtent2D actual{static_cast<uint32_t>(w), static_cast<uint32_t>(h)};
        actual.width  = std::clamp(actual.width,  caps.minImageExtent.width,  caps.maxImageExtent.width);
        actual.height = std::clamp(actual.height, caps.minImageExtent.height, caps.maxImageExtent.height);
        return actual;
    }

    /**
     * @brief Queries surface capabilities, formats and present modes for the physical device.
     */
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice dev) {
        SwapChainSupportDetails d;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(dev, surface, &d.capabilities);

        uint32_t fmtCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface, &fmtCount, nullptr);
        if (fmtCount) {
            d.formats.resize(fmtCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface, &fmtCount, d.formats.data());
        }

        uint32_t modeCount = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface, &modeCount, nullptr);
        if (modeCount) {
            d.presentModes.resize(modeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface, &modeCount, d.presentModes.data());
        }

        return d;
    }

    // -----------------------------------------------------------------------
    // Image views
    // -----------------------------------------------------------------------

    /**
     * @brief Creates one VkImageView for every swapchain image.
     *
     * A VkImageView describes how Vulkan should interpret a VkImage —
     * the GPU cannot use an image directly without a view.
     */
    void createImageViews() {
        swapChainImageViews.resize(swapChainImages.size());
        for (uint32_t i = 0; i < swapChainImages.size(); i++)
            swapChainImageViews[i] = createImageView(
                swapChainImages[i], swapChainImageFormat,
                VK_IMAGE_ASPECT_COLOR_BIT, 1);
    }

    // -----------------------------------------------------------------------
    // Render pass
    // -----------------------------------------------------------------------

    /**
     * @brief Describes the attachments and subpasses for one render pass.
     *
     * We use three attachments:
     *   0 — MSAA colour      (written by the rasteriser, resolved at end)
     *   1 — MSAA depth       (depth testing, not stored after the pass)
     *   2 — Resolve colour   (the actual swapchain image presented to screen)
     */
    void createRenderPass() {
        VkAttachmentDescription colorAtt{};
        colorAtt.format         = swapChainImageFormat;
        colorAtt.samples        = msaaSamples;
        colorAtt.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAtt.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        colorAtt.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAtt.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAtt.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAtt.finalLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentDescription depthAtt{};
        depthAtt.format         = findDepthFormat();
        depthAtt.samples        = msaaSamples;
        depthAtt.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAtt.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAtt.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAtt.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAtt.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAtt.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentDescription resolveAtt{};
        resolveAtt.format         = swapChainImageFormat;
        resolveAtt.samples        = VK_SAMPLE_COUNT_1_BIT;
        resolveAtt.loadOp         = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        resolveAtt.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        resolveAtt.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        resolveAtt.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        resolveAtt.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        resolveAtt.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkAttachmentReference depthRef{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
        VkAttachmentReference resolveRef{2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount    = 1;
        subpass.pColorAttachments       = &colorRef;
        subpass.pDepthStencilAttachment = &depthRef;
        subpass.pResolveAttachments     = &resolveRef;

        // Wait for colour output and early depth tests before this subpass reads/writes them
        VkSubpassDependency dep{};
        dep.srcSubpass    = VK_SUBPASS_EXTERNAL;
        dep.dstSubpass    = 0;
        dep.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                            VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dep.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        std::array<VkAttachmentDescription, 3> atts = {colorAtt, depthAtt, resolveAtt};
        VkRenderPassCreateInfo rpInfo{};
        rpInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rpInfo.attachmentCount = static_cast<uint32_t>(atts.size());
        rpInfo.pAttachments    = atts.data();
        rpInfo.subpassCount    = 1;
        rpInfo.pSubpasses      = &subpass;
        rpInfo.dependencyCount = 1;
        rpInfo.pDependencies   = &dep;

        if (vkCreateRenderPass(device, &rpInfo, nullptr, &renderPass) != VK_SUCCESS)
            throw std::runtime_error("failed to create render pass!");
    }

    // -----------------------------------------------------------------------
    // Descriptor set layout
    // -----------------------------------------------------------------------

    /**
     * @brief Declares what resources are bound to the shaders and at which binding points.
     *
     * Binding 0 — Uniform buffer (vertex shader: MVP matrices)
     * Binding 1 — Combined image sampler (fragment shader: texture)
     */
    void createDescriptorSetLayout() {
        VkDescriptorSetLayoutBinding uboBinding{};
        uboBinding.binding            = 0;
        uboBinding.descriptorCount    = 1;
        uboBinding.descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboBinding.stageFlags         = VK_SHADER_STAGE_VERTEX_BIT;
        uboBinding.pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutBinding samplerBinding{};
        samplerBinding.binding            = 1;
        samplerBinding.descriptorCount    = 1;
        samplerBinding.descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerBinding.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
        samplerBinding.pImmutableSamplers = nullptr;

        std::array<VkDescriptorSetLayoutBinding, 2> bindings = {uboBinding, samplerBinding};

        VkDescriptorSetLayoutCreateInfo ci{};
        ci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        ci.bindingCount = static_cast<uint32_t>(bindings.size());
        ci.pBindings    = bindings.data();

        if (vkCreateDescriptorSetLayout(device, &ci, nullptr, &descriptorSetLayout) != VK_SUCCESS)
            throw std::runtime_error("failed to create descriptor set layout!");
    }

    // -----------------------------------------------------------------------
    // Graphics pipeline
    // -----------------------------------------------------------------------

    /**
     * @brief Builds the full fixed-function + shader graphics pipeline.
     *
     * Loads pre-compiled SPIR-V from shaders/vert.spv and shaders/frag.spv,
     * wires up vertex input, input assembly, viewport, rasteriser, MSAA,
     * depth/stencil, colour blend and dynamic state, then creates the pipeline.
     * Shader modules are destroyed immediately after — they are only needed at
     * pipeline creation time.
     */
    void createGraphicsPipeline() {
        auto vertCode = readFile("shaders/vert.spv");
        auto fragCode = readFile("shaders/frag.spv");

        VkShaderModule vertModule = createShaderModule(vertCode);
        VkShaderModule fragModule = createShaderModule(fragCode);

        VkPipelineShaderStageCreateInfo vertStage{};
        vertStage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertStage.stage  = VK_SHADER_STAGE_VERTEX_BIT;
        vertStage.module = vertModule;
        vertStage.pName  = "main";

        VkPipelineShaderStageCreateInfo fragStage{};
        fragStage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragStage.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragStage.module = fragModule;
        fragStage.pName  = "main";

        VkPipelineShaderStageCreateInfo stages[] = {vertStage, fragStage};

        // --- Vertex input ---
        auto bindDesc  = Vertex::getBindingDescription();
        auto attrDescs = Vertex::getAttributeDescriptions();

        VkPipelineVertexInputStateCreateInfo vertexInput{};
        vertexInput.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInput.vertexBindingDescriptionCount   = 1;
        vertexInput.pVertexBindingDescriptions      = &bindDesc;
        vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrDescs.size());
        vertexInput.pVertexAttributeDescriptions    = attrDescs.data();

        // --- Input assembly ---
        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        // --- Viewport/scissor (dynamic) ---
        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount  = 1;

        // --- Rasteriser ---
        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable        = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode             = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth               = 1.0f;
        rasterizer.cullMode                = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.depthBiasEnable         = VK_FALSE;

        // --- Multisampling (MSAA) ---
        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable  = VK_FALSE;
        multisampling.rasterizationSamples = msaaSamples;

        // --- Depth/stencil ---
        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable       = VK_TRUE;
        depthStencil.depthWriteEnable      = VK_TRUE;
        depthStencil.depthCompareOp        = VK_COMPARE_OP_LESS;
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.stencilTestEnable     = VK_FALSE;

        // --- Colour blend (alpha blending disabled — opaque rendering) ---
        VkPipelineColorBlendAttachmentState blendAtt{};
        blendAtt.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                  VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        blendAtt.blendEnable    = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo colorBlend{};
        colorBlend.sType             = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlend.logicOpEnable     = VK_FALSE;
        colorBlend.logicOp           = VK_LOGIC_OP_COPY;
        colorBlend.attachmentCount   = 1;
        colorBlend.pAttachments      = &blendAtt;

        // --- Dynamic state (viewport & scissor set per command buffer) ---
        std::vector<VkDynamicState> dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates    = dynamicStates.data();

        // --- Pipeline layout (links descriptor set layout) ---
        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts    = &descriptorSetLayout;

        if (vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
            throw std::runtime_error("failed to create pipeline layout!");

        // --- Final pipeline ---
        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount          = 2;
        pipelineInfo.pStages             = stages;
        pipelineInfo.pVertexInputState   = &vertexInput;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState      = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState   = &multisampling;
        pipelineInfo.pDepthStencilState  = &depthStencil;
        pipelineInfo.pColorBlendState    = &colorBlend;
        pipelineInfo.pDynamicState       = &dynamicState;
        pipelineInfo.layout              = pipelineLayout;
        pipelineInfo.renderPass          = renderPass;
        pipelineInfo.subpass             = 0;
        pipelineInfo.basePipelineHandle  = VK_NULL_HANDLE;

        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS)
            throw std::runtime_error("failed to create graphics pipeline!");

        // Shader modules are only needed at pipeline creation time
        vkDestroyShaderModule(device, fragModule, nullptr);
        vkDestroyShaderModule(device, vertModule, nullptr);
    }

    // -----------------------------------------------------------------------
    // Framebuffers
    // -----------------------------------------------------------------------

    /**
     * @brief Creates one framebuffer per swapchain image.
     *
     * Each framebuffer binds the MSAA colour image, MSAA depth image and the
     * resolve (swapchain) image to the three render pass attachments.
     */
    void createFramebuffers() {
        swapChainFramebuffers.resize(swapChainImageViews.size());

        for (size_t i = 0; i < swapChainImageViews.size(); i++) {
            std::array<VkImageView, 3> atts = {
                colorImageView,
                depthImageView,
                swapChainImageViews[i]
            };

            VkFramebufferCreateInfo fi{};
            fi.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            fi.renderPass      = renderPass;
            fi.attachmentCount = static_cast<uint32_t>(atts.size());
            fi.pAttachments    = atts.data();
            fi.width           = swapChainExtent.width;
            fi.height          = swapChainExtent.height;
            fi.layers          = 1;

            if (vkCreateFramebuffer(device, &fi, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS)
                throw std::runtime_error("failed to create framebuffer!");
        }
    }

    // -----------------------------------------------------------------------
    // Command pool
    // -----------------------------------------------------------------------

    /**
     * @brief Creates the command pool for the graphics queue family.
     *
     * RESET_COMMAND_BUFFER_BIT lets us re-record individual command buffers
     * each frame without resetting the whole pool.
     */
    void createCommandPool() {
        QueueFamilyIndices idx = findQueueFamilies(physicalDevice);

        VkCommandPoolCreateInfo pi{};
        pi.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pi.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pi.queueFamilyIndex = idx.graphicsFamily.value();

        if (vkCreateCommandPool(device, &pi, nullptr, &commandPool) != VK_SUCCESS)
            throw std::runtime_error("failed to create graphics command pool!");
    }

    // -----------------------------------------------------------------------
    // MSAA colour resource
    // -----------------------------------------------------------------------

    /**
     * @brief Creates the multisampled colour image used as a render target.
     *
     * VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT hints to the driver that the
     * image contents don't need to be preserved between subpasses (tiled GPUs
     * may keep it on-chip entirely).
     */
    void createColorResources() {
        createImage(
            swapChainExtent.width, swapChainExtent.height, 1, msaaSamples,
            swapChainImageFormat, VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            colorImage, colorImageMemory);
        colorImageView = createImageView(colorImage, swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
    }

    // -----------------------------------------------------------------------
    // Depth buffer
    // -----------------------------------------------------------------------

    /**
     * @brief Creates the depth image used for per-fragment depth testing.
     */
    void createDepthResources() {
        VkFormat depthFmt = findDepthFormat();
        createImage(
            swapChainExtent.width, swapChainExtent.height, 1, msaaSamples,
            depthFmt, VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            depthImage, depthImageMemory);
        depthImageView = createImageView(depthImage, depthFmt, VK_IMAGE_ASPECT_DEPTH_BIT, 1);
    }

    /**
     * @brief Picks the best depth format the device supports.
     *
     * Tries 32-bit float depth, then 32-bit depth + 8-bit stencil, then
     * 24-bit depth + 8-bit stencil.
     */
    VkFormat findDepthFormat() {
        return findSupportedFormat(
            {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
    }

    /**
     * @brief Returns the first format from candidates that the device supports with the given tiling and features.
     */
    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates,
                                  VkImageTiling tiling, VkFormatFeatureFlags features) {
        for (VkFormat fmt : candidates) {
            VkFormatProperties props{};
            vkGetPhysicalDeviceFormatProperties(physicalDevice, fmt, &props);
            if (tiling == VK_IMAGE_TILING_LINEAR  && (props.linearTilingFeatures  & features) == features) return fmt;
            if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) return fmt;
        }
        throw std::runtime_error("failed to find supported format!");
    }

    /** @brief Returns true if the format includes a stencil component */
    bool hasStencilComponent(VkFormat fmt) {
        return fmt == VK_FORMAT_D32_SFLOAT_S8_UINT || fmt == VK_FORMAT_D24_UNORM_S8_UINT;
    }

    // -----------------------------------------------------------------------
    // Texture image
    // -----------------------------------------------------------------------

    /**
     * @brief Loads TEXTURE_PATH, uploads it via a staging buffer and generates mipmaps.
     *
     * mipLevels is derived from the larger of the two image dimensions.
     * The mipmap chain is generated on the GPU using vkCmdBlitImage.
     */
    void createTextureImage() {
        int w, h, ch;
        stbi_uc* pixels = stbi_load(TEXTURE_PATH.c_str(), &w, &h, &ch, STBI_rgb_alpha);
        if (!pixels)
            throw std::runtime_error("failed to load texture image!");

        VkDeviceSize size = static_cast<VkDeviceSize>(w) * h * 4;
        mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(w, h)))) + 1;

        VkBuffer       stagingBuf;
        VkDeviceMemory stagingMem;
        createBuffer(size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            stagingBuf, stagingMem);

        void* data;
        vkMapMemory(device, stagingMem, 0, size, 0, &data);
        memcpy(data, pixels, static_cast<size_t>(size));
        vkUnmapMemory(device, stagingMem);

        stbi_image_free(pixels);

        createImage(static_cast<uint32_t>(w), static_cast<uint32_t>(h),
            mipLevels, VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R8G8B8A8_SRGB,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            textureImage, textureImageMemory);

        transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mipLevels);
        copyBufferToImage(stagingBuf, textureImage, static_cast<uint32_t>(w), static_cast<uint32_t>(h));
        // Layout transitions to SHADER_READ_ONLY_OPTIMAL happen inside generateMipmaps

        vkDestroyBuffer(device, stagingBuf, nullptr);
        vkFreeMemory(device, stagingMem, nullptr);

        generateMipmaps(textureImage, VK_FORMAT_R8G8B8A8_SRGB, w, h, mipLevels);
    }

    /**
     * @brief Generates the full mipmap chain for an image on the GPU.
     *
     * Repeatedly blits each level to the next-smaller level using LINEAR filtering,
     * transitioning layouts along the way. The last level is transitioned to
     * SHADER_READ_ONLY_OPTIMAL after the loop.
     */
    void generateMipmaps(VkImage image, VkFormat fmt,
                          int32_t texW, int32_t texH, uint32_t levels) {
        VkFormatProperties props{};
        vkGetPhysicalDeviceFormatProperties(physicalDevice, fmt, &props);
        if (!(props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
            throw std::runtime_error("texture image format does not support linear blitting!");

        VkCommandBuffer cmd = beginSingleTimeCommands();

        VkImageMemoryBarrier barrier{};
        barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.image                           = image;
        barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount     = 1;
        barrier.subresourceRange.levelCount     = 1;

        int32_t mipW = texW, mipH = texH;

        for (uint32_t i = 1; i < levels; i++) {
            // Transition mip i-1 from DST_OPTIMAL to SRC_OPTIMAL (ready for blit source)
            barrier.subresourceRange.baseMipLevel = i - 1;
            barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            vkCmdPipelineBarrier(cmd,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                0, nullptr, 0, nullptr, 1, &barrier);

            VkImageBlit blit{};
            blit.srcOffsets[0] = {0, 0, 0};
            blit.srcOffsets[1] = {mipW, mipH, 1};
            blit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, i - 1, 0, 1};
            blit.dstOffsets[0] = {0, 0, 0};
            blit.dstOffsets[1] = {mipW > 1 ? mipW / 2 : 1, mipH > 1 ? mipH / 2 : 1, 1};
            blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, i, 0, 1};

            vkCmdBlitImage(cmd,
                image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1, &blit, VK_FILTER_LINEAR);

            // Transition mip i-1 to SHADER_READ_ONLY_OPTIMAL — it won't be written again
            barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(cmd,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                0, nullptr, 0, nullptr, 1, &barrier);

            if (mipW > 1) mipW /= 2;
            if (mipH > 1) mipH /= 2;
        }

        // Transition the last mip level (was never a blit source)
        barrier.subresourceRange.baseMipLevel = levels - 1;
        barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
            0, nullptr, 0, nullptr, 1, &barrier);

        endSingleTimeCommands(cmd);
    }

    /** @brief Returns the highest MSAA sample count supported by both colour and depth framebuffers */
    VkSampleCountFlagBits getMaxUsableSampleCount() {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(physicalDevice, &props);

        VkSampleCountFlags counts =
            props.limits.framebufferColorSampleCounts &
            props.limits.framebufferDepthSampleCounts;

        if (counts & VK_SAMPLE_COUNT_64_BIT) return VK_SAMPLE_COUNT_64_BIT;
        if (counts & VK_SAMPLE_COUNT_32_BIT) return VK_SAMPLE_COUNT_32_BIT;
        if (counts & VK_SAMPLE_COUNT_16_BIT) return VK_SAMPLE_COUNT_16_BIT;
        if (counts & VK_SAMPLE_COUNT_8_BIT)  return VK_SAMPLE_COUNT_8_BIT;
        if (counts & VK_SAMPLE_COUNT_4_BIT)  return VK_SAMPLE_COUNT_4_BIT;
        if (counts & VK_SAMPLE_COUNT_2_BIT)  return VK_SAMPLE_COUNT_2_BIT;
        return VK_SAMPLE_COUNT_1_BIT;
    }

    /** @brief Creates a VkImageView for the texture */
    void createTextureImageView() {
        textureImageView = createImageView(
            textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, mipLevels);
    }

    /**
     * @brief Creates the texture sampler with anisotropic filtering and mipmap support.
     *
     * maxAnisotropy is read from the device limits. maxLod = VK_LOD_CLAMP_NONE
     * means all mip levels are accessible.
     */
    void createTextureSampler() {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(physicalDevice, &props);

        VkSamplerCreateInfo si{};
        si.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        si.magFilter               = VK_FILTER_LINEAR;
        si.minFilter               = VK_FILTER_LINEAR;
        si.addressModeU            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        si.addressModeV            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        si.addressModeW            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        si.anisotropyEnable        = VK_TRUE;
        si.maxAnisotropy           = props.limits.maxSamplerAnisotropy;
        si.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        si.unnormalizedCoordinates = VK_FALSE;
        si.compareEnable           = VK_FALSE;
        si.compareOp               = VK_COMPARE_OP_ALWAYS;
        si.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        si.minLod                  = 0.0f;
        si.maxLod                  = VK_LOD_CLAMP_NONE;
        si.mipLodBias              = 0.0f;

        if (vkCreateSampler(device, &si, nullptr, &textureSampler) != VK_SUCCESS)
            throw std::runtime_error("failed to create texture sampler!");
    }

    // -----------------------------------------------------------------------
    // Image helpers
    // -----------------------------------------------------------------------

    /**
     * @brief Creates a VkImageView for any image with the specified format, aspect and mip range.
     */
    VkImageView createImageView(VkImage image, VkFormat format,
                                 VkImageAspectFlags aspect, uint32_t mips) {
        VkImageViewCreateInfo vi{};
        vi.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vi.image                           = image;
        vi.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        vi.format                          = format;
        vi.subresourceRange.aspectMask     = aspect;
        vi.subresourceRange.baseMipLevel   = 0;
        vi.subresourceRange.levelCount     = mips;
        vi.subresourceRange.baseArrayLayer = 0;
        vi.subresourceRange.layerCount     = 1;

        VkImageView view;
        if (vkCreateImageView(device, &vi, nullptr, &view) != VK_SUCCESS)
            throw std::runtime_error("failed to create image view!");
        return view;
    }

    /**
     * @brief Allocates and binds a VkImage and its device memory.
     */
    void createImage(uint32_t width, uint32_t height, uint32_t mips,
                     VkSampleCountFlagBits samples, VkFormat format,
                     VkImageTiling tiling, VkImageUsageFlags usage,
                     VkMemoryPropertyFlags props,
                     VkImage& image, VkDeviceMemory& memory) {
        VkImageCreateInfo ii{};
        ii.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        ii.imageType     = VK_IMAGE_TYPE_2D;
        ii.extent        = {width, height, 1};
        ii.mipLevels     = mips;
        ii.arrayLayers   = 1;
        ii.format        = format;
        ii.tiling        = tiling;
        ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        ii.usage         = usage;
        ii.samples       = samples;
        ii.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateImage(device, &ii, nullptr, &image) != VK_SUCCESS)
            throw std::runtime_error("failed to create image!");

        VkMemoryRequirements memReqs{};
        vkGetImageMemoryRequirements(device, image, &memReqs);

        VkMemoryAllocateInfo ai{};
        ai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        ai.allocationSize  = memReqs.size;
        ai.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, props);

        if (vkAllocateMemory(device, &ai, nullptr, &memory) != VK_SUCCESS)
            throw std::runtime_error("failed to allocate image memory!");

        vkBindImageMemory(device, image, memory, 0);
    }

    /**
     * @brief Issues a pipeline barrier to transition an image between layouts.
     *
     * Handles two transitions:
     *   UNDEFINED → TRANSFER_DST_OPTIMAL   (before upload)
     *   TRANSFER_DST_OPTIMAL → SHADER_READ_ONLY_OPTIMAL  (after upload)
     */
    void transitionImageLayout(VkImage image, VkFormat /*format*/,
                                VkImageLayout oldLayout, VkImageLayout newLayout,
                                uint32_t mips) {
        VkCommandBuffer cmd = beginSingleTimeCommands();

        VkImageMemoryBarrier barrier{};
        barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout                       = oldLayout;
        barrier.newLayout                       = newLayout;
        barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.image                           = image;
        barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel   = 0;
        barrier.subresourceRange.levelCount     = mips;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount     = 1;

        VkPipelineStageFlags srcStage, dstStage;

        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
            newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
                   newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        } else {
            throw std::invalid_argument("unsupported layout transition!");
        }

        vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0,
            0, nullptr, 0, nullptr, 1, &barrier);

        endSingleTimeCommands(cmd);
    }

    /** @brief Copies a buffer into an image (used when uploading texture data) */
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t w, uint32_t h) {
        VkCommandBuffer cmd = beginSingleTimeCommands();

        VkBufferImageCopy region{};
        region.bufferOffset                    = 0;
        region.bufferRowLength                 = 0;
        region.bufferImageHeight               = 0;
        region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel       = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount     = 1;
        region.imageOffset                     = {0, 0, 0};
        region.imageExtent                     = {w, h, 1};

        vkCmdCopyBufferToImage(cmd, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        endSingleTimeCommands(cmd);
    }

    // -----------------------------------------------------------------------
    // Model loading
    // -----------------------------------------------------------------------

    /**
     * @brief Loads MODEL_PATH using tinyobjloader and deduplicates vertices.
     *
     * Vertices with identical position/colour/UV are merged via an unordered_map.
     * The Y texture coordinate is flipped to match Vulkan's top-left origin.
     */
    void loadModel() {
        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t>    shapes;
        std::vector<tinyobj::material_t> materials;
        std::string err;

        if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &err, MODEL_PATH.c_str()))
            throw std::runtime_error(err);

        std::unordered_map<Vertex, uint32_t> uniqueVerts{};

        for (const auto& shape : shapes) {
            for (const auto& index : shape.mesh.indices) {
                Vertex v{};
                v.pos = {
                    attrib.vertices[3 * index.vertex_index + 0],
                    attrib.vertices[3 * index.vertex_index + 1],
                    attrib.vertices[3 * index.vertex_index + 2]
                };
                v.texCoord = {
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
                };
                v.color = {1.0f, 1.0f, 1.0f};

                if (!uniqueVerts.count(v)) {
                    uniqueVerts[v] = static_cast<uint32_t>(vertices.size());
                    vertices.push_back(v);
                }
                indices.push_back(uniqueVerts[v]);
            }
        }
    }

    // -----------------------------------------------------------------------
    // Buffers
    // -----------------------------------------------------------------------

    /**
     * @brief Uploads vertex data to a device-local GPU buffer via a staging buffer.
     *
     * The staging buffer lives in host-visible memory (CPU can write it).
     * vkCmdCopyBuffer transfers it to device-local memory (fastest for the GPU).
     */
    void createVertexBuffer() {
        VkDeviceSize size = sizeof(vertices[0]) * vertices.size();

        VkBuffer staging; VkDeviceMemory stagingMem;
        createBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            staging, stagingMem);

        void* data;
        vkMapMemory(device, stagingMem, 0, size, 0, &data);
        memcpy(data, vertices.data(), static_cast<size_t>(size));
        vkUnmapMemory(device, stagingMem);

        createBuffer(size,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            vertexBuffer, vertexBufferMemory);

        copyBuffer(staging, vertexBuffer, size);
        vkDestroyBuffer(device, staging, nullptr);
        vkFreeMemory(device, stagingMem, nullptr);
    }

    /** @brief Same as createVertexBuffer but for index data */
    void createIndexBuffer() {
        VkDeviceSize size = sizeof(indices[0]) * indices.size();

        VkBuffer staging; VkDeviceMemory stagingMem;
        createBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            staging, stagingMem);

        void* data;
        vkMapMemory(device, stagingMem, 0, size, 0, &data);
        memcpy(data, indices.data(), static_cast<size_t>(size));
        vkUnmapMemory(device, stagingMem);

        createBuffer(size,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            indexBuffer, indexBufferMemory);

        copyBuffer(staging, indexBuffer, size);
        vkDestroyBuffer(device, staging, nullptr);
        vkFreeMemory(device, stagingMem, nullptr);
    }

    /**
     * @brief Creates one persistently-mapped uniform buffer per frame-in-flight.
     *
     * These stay mapped for the lifetime of the app (persistent mapping).
     * HOST_COHERENT_BIT means we don't need to flush after writing.
     */
    void createUniformBuffers() {
        VkDeviceSize size = sizeof(UniformBufferObject);
        uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
        uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            createBuffer(size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                uniformBuffers[i], uniformBuffersMemory[i]);
            vkMapMemory(device, uniformBuffersMemory[i], 0, size, 0, &uniformBuffersMapped[i]);
        }
    }

    /**
     * @brief Allocates and binds a VkBuffer + VkDeviceMemory of the requested size and type.
     */
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                      VkMemoryPropertyFlags props,
                      VkBuffer& buffer, VkDeviceMemory& memory) {
        VkBufferCreateInfo bi{};
        bi.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bi.size        = size;
        bi.usage       = usage;
        bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(device, &bi, nullptr, &buffer) != VK_SUCCESS)
            throw std::runtime_error("failed to create buffer!");

        VkMemoryRequirements memReqs{};
        vkGetBufferMemoryRequirements(device, buffer, &memReqs);

        VkMemoryAllocateInfo ai{};
        ai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        ai.allocationSize  = memReqs.size;
        ai.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, props);

        if (vkAllocateMemory(device, &ai, nullptr, &memory) != VK_SUCCESS)
            throw std::runtime_error("failed to allocate buffer memory!");

        vkBindBufferMemory(device, buffer, memory, 0);
    }

    /** @brief Copies size bytes from srcBuffer to dstBuffer using a one-shot command buffer */
    void copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size) {
        VkCommandBuffer cmd = beginSingleTimeCommands();
        VkBufferCopy region{};
        region.size = size;
        vkCmdCopyBuffer(cmd, src, dst, 1, &region);
        endSingleTimeCommands(cmd);
    }

    /**
     * @brief Scans the device memory heaps and returns the index of the first type that
     *        satisfies typeFilter and has all requested property flags.
     */
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags props) {
        VkPhysicalDeviceMemoryProperties memProps{};
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);

        for (uint32_t i = 0; i < memProps.memoryTypeCount; i++)
            if ((typeFilter & (1u << i)) &&
                (memProps.memoryTypes[i].propertyFlags & props) == props)
                return i;

        throw std::runtime_error("failed to find suitable memory type!");
    }

    // -----------------------------------------------------------------------
    // Descriptors
    // -----------------------------------------------------------------------

    /**
     * @brief Creates a descriptor pool large enough for MAX_FRAMES_IN_FLIGHT descriptor sets
     *        each containing one UBO and one combined image sampler.
     */
    void createDescriptorPool() {
        std::array<VkDescriptorPoolSize, 2> sizes{};
        sizes[0].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        sizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        sizes[1].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        sizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

        VkDescriptorPoolCreateInfo pi{};
        pi.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pi.poolSizeCount = static_cast<uint32_t>(sizes.size());
        pi.pPoolSizes    = sizes.data();
        pi.maxSets       = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

        if (vkCreateDescriptorPool(device, &pi, nullptr, &descriptorPool) != VK_SUCCESS)
            throw std::runtime_error("failed to create descriptor pool!");
    }

    /**
     * @brief Allocates one descriptor set per frame and writes the UBO and sampler into each.
     */
    void createDescriptorSets() {
        std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);

        VkDescriptorSetAllocateInfo ai{};
        ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        ai.descriptorPool     = descriptorPool;
        ai.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        ai.pSetLayouts        = layouts.data();

        descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
        if (vkAllocateDescriptorSets(device, &ai, descriptorSets.data()) != VK_SUCCESS)
            throw std::runtime_error("failed to allocate descriptor sets!");

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            VkDescriptorBufferInfo bufInfo{};
            bufInfo.buffer = uniformBuffers[i];
            bufInfo.offset = 0;
            bufInfo.range  = sizeof(UniformBufferObject);

            VkDescriptorImageInfo imgInfo{};
            imgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imgInfo.imageView   = textureImageView;
            imgInfo.sampler     = textureSampler;

            std::array<VkWriteDescriptorSet, 2> writes{};

            writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[0].dstSet          = descriptorSets[i];
            writes[0].dstBinding      = 0;
            writes[0].dstArrayElement = 0;
            writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writes[0].descriptorCount = 1;
            writes[0].pBufferInfo     = &bufInfo;

            writes[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[1].dstSet          = descriptorSets[i];
            writes[1].dstBinding      = 1;
            writes[1].dstArrayElement = 0;
            writes[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[1].descriptorCount = 1;
            writes[1].pImageInfo      = &imgInfo;

            vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
        }
    }

    // -----------------------------------------------------------------------
    // Command buffers
    // -----------------------------------------------------------------------

    /** @brief Allocates one primary command buffer per frame-in-flight */
    void createCommandBuffers() {
        commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

        VkCommandBufferAllocateInfo ai{};
        ai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        ai.commandPool        = commandPool;
        ai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ai.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

        if (vkAllocateCommandBuffers(device, &ai, commandBuffers.data()) != VK_SUCCESS)
            throw std::runtime_error("failed to allocate command buffers!");
    }

    /**
     * @brief Records the render pass and draw calls for a single frame.
     *
     * Sets dynamic viewport/scissor, binds the pipeline, vertex/index buffers
     * and descriptor sets, then issues an indexed draw call.
     */
    void recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex) {
        VkCommandBufferBeginInfo bi{};
        bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(cmd, &bi) != VK_SUCCESS)
            throw std::runtime_error("failed to begin recording command buffer!");

        VkRenderPassBeginInfo rpInfo{};
        rpInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpInfo.renderPass        = renderPass;
        rpInfo.framebuffer       = swapChainFramebuffers[imageIndex];
        rpInfo.renderArea.offset = {0, 0};
        rpInfo.renderArea.extent = swapChainExtent;

        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color        = {{0.0f, 0.0f, 0.0f, 1.0f}};
        clearValues[1].depthStencil = {1.0f, 0};

        rpInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        rpInfo.pClearValues    = clearValues.data();

        vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

            VkViewport vp{};
            vp.x        = 0.0f;
            vp.y        = 0.0f;
            vp.width    = static_cast<float>(swapChainExtent.width);
            vp.height   = static_cast<float>(swapChainExtent.height);
            vp.minDepth = 0.0f;
            vp.maxDepth = 1.0f;
            vkCmdSetViewport(cmd, 0, 1, &vp);

            VkRect2D scissor{};
            scissor.offset = {0, 0};
            scissor.extent = swapChainExtent;
            vkCmdSetScissor(cmd, 0, 1, &scissor);

            VkBuffer     vbos[]     = {vertexBuffer};
            VkDeviceSize offsets[]  = {0};
            vkCmdBindVertexBuffers(cmd, 0, 1, vbos, offsets);
            vkCmdBindIndexBuffer(cmd, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                pipelineLayout, 0, 1, &descriptorSets[currentFrame], 0, nullptr);

            vkCmdDrawIndexed(cmd, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

        vkCmdEndRenderPass(cmd);

        if (vkEndCommandBuffer(cmd) != VK_SUCCESS)
            throw std::runtime_error("failed to record command buffer!");
    }

    // -----------------------------------------------------------------------
    // Synchronisation
    // -----------------------------------------------------------------------

    /**
     * @brief Creates semaphores and fences for MAX_FRAMES_IN_FLIGHT.
     *
     * imageAvailableSemaphore — GPU waits for a swapchain image to be free
     * renderFinishedSemaphore — GPU signals when rendering is done
     * inFlightFence           — CPU waits here so it doesn't overwrite buffers in use
     * Fences are created in the signalled state so the first frame doesn't stall.
     */
    void createSyncObjects() {
        imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

        VkSemaphoreCreateInfo si{};
        si.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fi{};
        fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fi.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if (vkCreateSemaphore(device, &si, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
                vkCreateSemaphore(device, &si, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
                vkCreateFence    (device, &fi, nullptr, &inFlightFences[i])            != VK_SUCCESS)
                throw std::runtime_error("failed to create synchronization objects for a frame!");
        }
    }

    // -----------------------------------------------------------------------
    // Per-frame update & draw
    // -----------------------------------------------------------------------

    /**
     * @brief Writes updated MVP matrices into the current frame's uniform buffer.
     *
     * The model rotates continuously. The projection matrix Y flip compensates
     * for the difference between OpenGL (Y-up) and Vulkan (Y-down) clip space.
     */
    void updateUniformBuffer(uint32_t frame) {
        static auto start = std::chrono::high_resolution_clock::now();
        auto now   = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(now - start).count();

        UniformBufferObject ubo{};
        ubo.model = glm::rotate(glm::mat4(1.0f),
                        time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.view  = glm::lookAt(
                        glm::vec3(2.0f, 2.0f, 2.0f),
                        glm::vec3(0.0f, 0.0f, 0.0f),
                        glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.proj  = glm::perspective(
                        glm::radians(45.0f),
                        swapChainExtent.width / static_cast<float>(swapChainExtent.height),
                        0.1f, 10.0f);
        ubo.proj[1][1] *= -1; // Flip Y for Vulkan NDC

        memcpy(uniformBuffersMapped[frame], &ubo, sizeof(ubo));
    }

    /**
     * @brief Acquires a swapchain image, submits rendering commands, and presents.
     *
     * The flow each frame:
     *   1. Wait for the previous use of this frame slot to finish (fence)
     *   2. Acquire the next swapchain image (may recreate swapchain if needed)
     *   3. Update the uniform buffer for this frame
     *   4. Reset + record the command buffer
     *   5. Submit to the graphics queue (semaphore-synchronised)
     *   6. Present the finished image (recreate swapchain if out-of-date)
     *   7. Advance to the next frame slot
     */
    void drawFrame() {
        vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(
            device, swapChain, UINT64_MAX,
            imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapChain();
            return;
        } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("failed to acquire swap chain image!");
        }

        updateUniformBuffer(currentFrame);
        vkResetFences(device, 1, &inFlightFences[currentFrame]);

        vkResetCommandBuffer(commandBuffers[currentFrame], 0);
        recordCommandBuffer(commandBuffers[currentFrame], imageIndex);

        VkSemaphore          waitSems[]   = {imageAvailableSemaphores[currentFrame]};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        VkSemaphore          signalSems[] = {renderFinishedSemaphores[currentFrame]};

        VkSubmitInfo submitInfo{};
        submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount   = 1;
        submitInfo.pWaitSemaphores      = waitSems;
        submitInfo.pWaitDstStageMask    = waitStages;
        submitInfo.commandBufferCount   = 1;
        submitInfo.pCommandBuffers      = &commandBuffers[currentFrame];
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores    = signalSems;

        if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS)
            throw std::runtime_error("failed to submit draw command buffer!");

        VkSwapchainKHR swapChains[] = {swapChain};

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores    = signalSems;
        presentInfo.swapchainCount     = 1;
        presentInfo.pSwapchains        = swapChains;
        presentInfo.pImageIndices      = &imageIndex;

        result = vkQueuePresentKHR(presentQueue, &presentInfo);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
            framebufferResized = false;
            recreateSwapChain();
        } else if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to present swap chain image!");
        }

        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    // -----------------------------------------------------------------------
    // One-shot command buffer helpers
    // -----------------------------------------------------------------------

    /** @brief Allocates and begins a temporary command buffer for transfer/blit operations */
    VkCommandBuffer beginSingleTimeCommands() {
        VkCommandBufferAllocateInfo ai{};
        ai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        ai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ai.commandPool        = commandPool;
        ai.commandBufferCount = 1;

        VkCommandBuffer cmd;
        vkAllocateCommandBuffers(device, &ai, &cmd);

        VkCommandBufferBeginInfo bi{};
        bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &bi);

        return cmd;
    }

    /** @brief Ends, submits and frees a one-shot command buffer */
    void endSingleTimeCommands(VkCommandBuffer cmd) {
        vkEndCommandBuffer(cmd);

        VkSubmitInfo si{};
        si.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        si.commandBufferCount = 1;
        si.pCommandBuffers    = &cmd;

        vkQueueSubmit(graphicsQueue, 1, &si, VK_NULL_HANDLE);
        vkQueueWaitIdle(graphicsQueue);

        vkFreeCommandBuffers(device, commandPool, 1, &cmd);
    }

    // -----------------------------------------------------------------------
    // Queue families
    // -----------------------------------------------------------------------

    /**
     * @brief Finds queue family indices for graphics commands and surface presentation.
     */
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice dev) {
        QueueFamilyIndices idx;

        uint32_t count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, nullptr);
        std::vector<VkQueueFamilyProperties> families(count);
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, families.data());

        int i = 0;
        for (const auto& qf : families) {
            if (qf.queueFlags & VK_QUEUE_GRAPHICS_BIT)
                idx.graphicsFamily = i;

            VkBool32 present = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface, &present);
            if (present)
                idx.presentFamily = i;

            if (idx.isComplete()) break;
            i++;
        }
        return idx;
    }

    // -----------------------------------------------------------------------
    // Extensions & layers
    // -----------------------------------------------------------------------

    /**
     * @brief Returns the extensions GLFW requires plus the debug utils extension (if needed).
     */
    std::vector<const char*> getRequiredExtensions() {
        uint32_t     glfwCount = 0;
        const char** glfwExts  = glfwGetRequiredInstanceExtensions(&glfwCount);

        std::vector<const char*> exts(glfwExts, glfwExts + glfwCount);
        if (enableValidationLayers)
            exts.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        return exts;
    }

    /**
     * @brief Checks that every layer in validationLayers is available on the system.
     */
    bool checkValidationLayerSupport() {
        uint32_t count = 0;
        vkEnumerateInstanceLayerProperties(&count, nullptr);
        std::vector<VkLayerProperties> available(count);
        vkEnumerateInstanceLayerProperties(&count, available.data());

        for (const char* name : validationLayers) {
            bool found = false;
            for (const auto& lp : available)
                if (strcmp(name, lp.layerName) == 0) { found = true; break; }
            if (!found) return false;
        }
        return true;
    }

    // -----------------------------------------------------------------------
    // Shader & file helpers
    // -----------------------------------------------------------------------

    /**
     * @brief Reads an entire binary file into a vector<char>.
     *
     * Opens at the end (ios::ate) to get the file size, then reads from the start.
     */
    static std::vector<char> readFile(const std::string& path) {
        std::ifstream file(path, std::ios::ate | std::ios::binary);
        if (!file.is_open())
            throw std::runtime_error("failed to open file: " + path);

        size_t size = static_cast<size_t>(file.tellg());
        std::vector<char> buf(size);
        file.seekg(0);
        file.read(buf.data(), static_cast<std::streamsize>(size));
        return buf;
    }

    /**
     * @brief Wraps SPIR-V bytecode in a VkShaderModule.
     *
     * The reinterpret_cast is safe here because std::vector guarantees
     * its storage satisfies the alignment requirement of uint32_t.
     */
    VkShaderModule createShaderModule(const std::vector<char>& code) {
        VkShaderModuleCreateInfo ci{};
        ci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        ci.codeSize = code.size();
        ci.pCode    = reinterpret_cast<const uint32_t*>(code.data());

        VkShaderModule mod;
        if (vkCreateShaderModule(device, &ci, nullptr, &mod) != VK_SUCCESS)
            throw std::runtime_error("failed to create shader module!");
        return mod;
    }

    // -----------------------------------------------------------------------
    // Debug callback
    // -----------------------------------------------------------------------

    /**
     * @brief Validation layer callback — prints the message to stderr.
     *
     * Returns VK_FALSE to indicate the API call that triggered this should not be aborted.
     */
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT    /*severity*/,
        VkDebugUtilsMessageTypeFlagsEXT           /*type*/,
        const VkDebugUtilsMessengerCallbackDataEXT* pData,
        void* /*pUserData*/)
    {
        std::cerr << "validation layer: " << pData->pMessage << '\n';
        return VK_FALSE;
    }
};

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

int main() {
    CRF app;
    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
