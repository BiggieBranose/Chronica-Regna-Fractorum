/**
 * @file main.cpp
 * @brief Minimal Vulkan + GLFW application using Vulkan-Hpp RAII wrappers.
 */

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>

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

    /**
     * @brief Initializes Vulkan components.
     *
     * Currently only creates the Vulkan instance, but will later include
     * physical device selection, logical device creation, etc.
     */
    void initVulkan()
    {
        createInstance();
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
            "CRF",                         ///< Application name
            VK_MAKE_VERSION(1, 0, 0),      ///< Application version
            "Branose Engine",              ///< Engine name
            VK_MAKE_VERSION(1, 0, 0),      ///< Engine version
            VK_API_VERSION_1_4             ///< Requested Vulkan API version
        );

        // get required layers
        std::vector<char const*> requiredLayers;
        if (enableValidationLayers)
        {
            requiredLayers.assign(validationLayers.begin(), validationLayers.end());
        }

        // Check if the layers are supported by the Vulkan implementation
        auto layerProperties = context.enumerateInstanceLayerProperties();
        auto unsupportedLayerIt = std::ranges::find_if(requiredLayers,
                                                        [&layerProperties](auto const &requiredLayer) {
                                                            return std::ranges::none_of(layerProperties,
                                                                                            [requiredLayer](auto const &layerProperty) { return strcmp(layerProperty.layerName, requiredLayer) == 0; });
                                                        });
        if (unsupportedLayerIt != requiredLayers.end())
        {
            throw std::runtime_error("Required layer not supported: " + std::string(*unsupportedLayerIt));
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

        // Retrieve required instance extensions from GLFW.
        uint32_t glfwExtensionCount = 0;
        auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        // Validate that GLFW-required extensions exist.
        for (uint32_t i = 0; i < glfwExtensionCount; ++i)
        {
            // Explanation: std::ranges::none_of checks if no extension matches the required one.
            if (std::ranges::none_of(
                    extensionProperties,
                    [glfwExtension = glfwExtensions[i]](auto const& extensionProperty)
                    {
                        return strcmp(extensionProperty.extensionName, glfwExtension) == 0;
                    }))
            {
                throw std::runtime_error(
                    "Required GLFW extension not supported: " +
                    std::string(glfwExtensions[i])
                );
            }
        }

        vk::InstanceCreateInfo createInfo(
            {},                 ///< Flags (unused)
            &appInfo,           ///< Application info
            0,                  ///< Layer count
            nullptr,            ///< Layer names
            glfwExtensionCount, ///< Extension count
            glfwExtensions      ///< Extension names
        );
        instance = vk::raii::Instance(context, createInfo);
    }

    std::vector<const char *> getRequiredInstanceExtensions()
    {
        uint32_t glfwExtensionCount = 0;
        auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        return extensions;
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
}
