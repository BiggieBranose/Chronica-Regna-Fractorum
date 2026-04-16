/**
 * @file main.cpp
 * @brief Minimal Vulkan + GLFW application using Vulkan-Hpp RAII wrappers.
 */

#include "vulkan/vulkan.hpp"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vulkan/vk_platform.h>

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
 * - Main event loop
 * - Cleanup of resources
 */
class HelloTriangleApplication
{
public:
    /**
     * @brief Runs the full application lifecycle.
     *
     * Calls initialization, enters the main loop, and performs cleanup.
     */
    void run()
    {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:
    GLFWwindow* window = nullptr; ///< Pointer to the GLFW window.

    vk::raii::Context  context;   ///< Vulkan-Hpp RAII context.
    vk::raii::Instance instance = nullptr; ///< Vulkan instance handle.
    vk::raii::DebugUtilsMessengerEXT debugMSG = nullptr; ///< debug messenger instance handle.
    vk::raii::PhysicalDevice phyDevice = nullptr;

    /**
     * @brief Initializes the GLFW window.
     *
     * Sets GLFW to not create an OpenGL context and disables resizing.
     */
    void initWindow()
    {
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // Vulkan requires no OpenGL context.
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);   // Simplicity: disable resizing.

        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
    }

    /** @brief Initializes Vulkan components. */
    void initVulkan()
    {
        createInstance();
        setupDebugMessenger();
        pickPhysicalDevice();
    }

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

        vk::DebugUtilsMessengerCreateInfoEXT createInfo(
            {},                 // flags (always zero)
            severityFlags,      // messageSeverity
            messageTypeFlags,   // messageType
            &debugCallback,     // pfnUserCallback
            nullptr             // pUserData
        );

        debugMSG = instance.createDebugUtilsMessengerEXT(createInfo);
    }

    void pickPhysicalDevice()
    {
        auto phyDevices = instance.enumeratePhysicalDevices();
        if (phyDevices.empty())
        {
            throw std::runtime_error("failed to find GPUs with Vulkan support!");
        }

        /*for (phyDevice : phyDevices)
        {
            break;
        }*/
    }

    bool isDeviceSuitable(vk::raii::PhysicalDevice const & physicalDevice)
    {
        auto deviceProperties = physicalDevice.getProperties();
        auto deviceFeatures = physicalDevice.getFeatures();

        if (deviceProperties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu && deviceFeatures.geometryShader) {
            return true;
        }

        return false;
    }


    /**
     * @brief Main application loop.
     *
     * Polls window events until the user closes the window.
     */
    void mainLoop()
    {
        while (!glfwWindowShouldClose(window))
        {
            glfwPollEvents();
        }
    }

    /**
     * @brief Cleans up GLFW and Vulkan resources.
     *
     * Vulkan RAII objects clean themselves up automatically.
     */
    void cleanup()
    {
        glfwDestroyWindow(window);
        glfwTerminate();
    }

    /**
     * @brief Creates the Vulkan instance.
     *
     * This function:
     * - Lists required validation layers
     * - Check if the layers are supported by the Vulkan implementation
     * - Defines application metadata
     * - Retrieves required GLFW extensions
     * - Validates that the Vulkan implementation supports them
     * - Creates a Vulkan instance using Vulkan-Hpp RAII
     *
     * @throws std::runtime_error if a required extension is missing.
     */
    void createInstance()
    {
        constexpr vk::ApplicationInfo appInfo(
                "CRF",                               // pApplicationName
                VK_MAKE_VERSION(1, 0, 0),            // applicationVersion
                "Branose Engine",                    // pEngineName
                VK_MAKE_VERSION(1, 1, 0),            // engineVersion
                vk::ApiVersion14                     // apiVersion
        );

        // Get the required layers
        std::vector<char const*> requiredLayers;
        if (enableValidationLayers) {
            requiredLayers.assign(validationLayers.begin(), validationLayers.end());
        }

        // Check if the required layers are supported by the Vulkan implementation.
        auto layerProperties = context.enumerateInstanceLayerProperties();
        if (std::ranges::any_of(requiredLayers, [&layerProperties](auto const& requiredLayer) {
            return std::ranges::none_of(layerProperties,
                                    [requiredLayer](auto const& layerProperty)
                                    { return strcmp(layerProperty.layerName, requiredLayer) == 0; });
        }))
        {
            throw std::runtime_error("One or more required layers are not supported!");
        }

        // Get the required extensions.
        auto requiredExtensions = getRequiredInstanceExtensions();

        // Check if the required extensions are supported by the Vulkan implementation.
        auto extensionProperties = context.enumerateInstanceExtensionProperties();
            auto unsupportedPropertyIt =
                std::ranges::find_if(requiredExtensions,
                                    [&extensionProperties](auto const &requiredExtension) {
                                        return std::ranges::none_of(extensionProperties,
                                                                    [requiredExtension](auto const &extensionProperty) { return strcmp(extensionProperty.extensionName, requiredExtension) == 0; });
                                    });
            if (unsupportedPropertyIt != requiredExtensions.end())
            {
                throw std::runtime_error("Required extension not supported: " + std::string(*unsupportedPropertyIt));
            }

        vk::InstanceCreateInfo createInfo{
            .pApplicationInfo        = &appInfo,
            .enabledLayerCount       = static_cast<uint32_t>(requiredLayers.size()),
            .ppEnabledLayerNames     = requiredLayers.data(),
            .enabledExtensionCount   = static_cast<uint32_t>(requiredExtensions.size()),
            .ppEnabledExtensionNames = requiredExtensions.data() };
        instance = vk::raii::Instance(context, createInfo);
    }

    /** @brief gets the required extentions and prints some debugging if validation layers are enabled */
    std::vector<const char *> getRequiredInstanceExtensions()
    {
        uint32_t glfwExtensionCount = 0;
        auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
        if (enableValidationLayers)
        {
            extensions.push_back(vk::EXTDebugUtilsExtensionName);
        }

        return extensions;
    }

    /**
    * @brief Vulkan debug messenger callback.
    *
    * This function is invoked by the Vulkan validation layers whenever a
    * diagnostic message is generated. It prints the message to stderr.
    *
    * @param severity
    *     The severity level of the message (verbose, info, warning, error).
    *
    * @param type
    *     The type of message (general, validation, performance).
    *
    * @param pCallbackData
    *     Pointer to a structure containing detailed debug information,
    *     including the human‑readable message string.
    *
    * @param pUserData
    *     Optional user data pointer supplied during messenger creation.
    *     Unused in this implementation.
    *
    * @return vk::False
    *     Returning `vk::False` tells Vulkan that the call should not be aborted.
    *     (Returning `vk::True` would indicate that the validation layer should
    *     halt the Vulkan call that triggered the message.)
    *
    * @note
    *     This callback is compatible with Vulkan-Hpp RAII and uses the
    *     `VKAPI_ATTR` and `VKAPI_CALL` macros to ensure correct calling
    *     conventions across platforms.
    */
    static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(
        vk::DebugUtilsMessageSeverityFlagBitsEXT       severity,
        vk::DebugUtilsMessageTypeFlagsEXT              type,
        const vk::DebugUtilsMessengerCallbackDataEXT*  pCallbackData,
        void*                                          pUserData)
    {
        // Normal comment: Vulkan guarantees pCallbackData->pMessage is a null‑terminated C string.
        std::cerr << "validation layer: type "
                << " msg: " << pCallbackData->pMessage
                << std::endl;

        return vk::False; // Normal comment: Do not stop Vulkan calls.

    }
};

/**
 * @brief Application entry point.
 *
 * Creates and runs the HelloTriangleApplication instance.
 *
 * @return EXIT_SUCCESS on success, EXIT_FAILURE on error.
 */
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
};
