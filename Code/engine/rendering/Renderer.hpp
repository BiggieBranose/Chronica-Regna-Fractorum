#pragma once
#include <vulkan/vulkan_raii.hpp>
#include <GLFW/glfw3.h>
#include <memory>
#include <vector>
#include <array>

typedef struct VmaAllocator_T* VmaAllocator;
typedef struct VmaAllocation_T* VmaAllocation;

namespace crf {

class Camera;
class Texture;
class SpriteRenderer;

class Renderer {
public:
    Renderer();
    ~Renderer();

    bool initialize(GLFWwindow* window);
    void shutdown();

    bool beginFrame();
    void endFrame();

    void onResize();

    VkDevice getDevice() const { return *m_device; }
    VkInstance getInstance() const { return *m_instance; }
    VkPhysicalDevice getPhysicalDevice() const { return *m_physicalDevice; }
    VmaAllocator getAllocator() const { return toVma(m_vmaAllocator); }
    vk::raii::CommandPool& getCommandPool() { return m_commandPool; }
    vk::raii::Queue& getQueue() { return m_queue; }
    uint32_t getQueueIndex() const { return m_queueIndex; }
    vk::Format getColorFormat() const { return m_surfaceFormat.format; }
    vk::Extent2D getExtent() const { return m_extent; }
    uint32_t getCurrentFrame() const { return m_frameIndex; }
    uint32_t getImageCount() const { return static_cast<uint32_t>(m_swapchainImages.size()); }
    vk::raii::CommandBuffer& getCurrentCommandBuffer() { return m_graphicsCommandBuffers[m_frameIndex]; }
    const std::vector<vk::Image>& getSwapchainImages() const { return m_swapchainImages; }
    const std::vector<vk::raii::ImageView>& getImageViews() const { return m_imageViews; }
    VkDescriptorPool getImGuiDescriptorPool() const { return *m_imguiDescriptorPool; }

    SpriteRenderer& getSpriteRenderer() { return *m_spriteRenderer; }

private:
    // Vulkan state
    vk::raii::Context m_context;
    vk::raii::Instance m_instance = nullptr;
    vk::raii::SurfaceKHR m_surface = nullptr;
    vk::raii::DebugUtilsMessengerEXT m_debugMessenger = nullptr;

    vk::raii::PhysicalDevice m_physicalDevice = nullptr;
    vk::raii::Device m_device = nullptr;
    vk::raii::Queue m_queue = nullptr;
    uint32_t m_queueIndex = 0;

    void* m_vmaAllocator = nullptr;

    vk::raii::SwapchainKHR m_swapchain = nullptr;
    std::vector<vk::Image> m_swapchainImages;
    std::vector<vk::raii::ImageView> m_imageViews;
    vk::SurfaceFormatKHR m_surfaceFormat{};
    vk::Extent2D m_extent{};

    vk::raii::CommandPool m_commandPool = nullptr;
    std::vector<vk::raii::CommandBuffer> m_graphicsCommandBuffers;

    std::vector<vk::raii::Semaphore> m_imageAvailableSemaphores;
    std::vector<vk::raii::Fence> m_inFlightFences;

    vk::raii::DescriptorPool m_imguiDescriptorPool = nullptr;
    std::unique_ptr<SpriteRenderer> m_spriteRenderer;

    uint32_t m_frameIndex = 0;
    uint32_t m_currentImageIndex = 0;
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
    GLFWwindow* m_window = nullptr;
    bool m_framebufferResized = false;

    static VmaAllocator toVma(void* p) { return static_cast<VmaAllocator>(p); }

    bool initInstanceAndSurface();
    bool initDevice();
    bool initSwapchain();
    bool initCommands();
    bool initSpriteRenderer();
    void destroySwapchain();
    void destroyAll();

    void createSyncObjects();
    void recordCommandBuffer(uint32_t imageIndex);
    void transitionImageLayout(uint32_t imageIndex, vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
                               vk::AccessFlags2 srcAccess, vk::AccessFlags2 dstAccess,
                               vk::PipelineStageFlags2 srcStage, vk::PipelineStageFlags2 dstStage,
                               vk::Image image);

    static void framebufferResizeCallback(GLFWwindow* window, int, int);
};

} // namespace crf
