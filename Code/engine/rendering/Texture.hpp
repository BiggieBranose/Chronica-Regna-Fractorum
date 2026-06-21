#pragma once
#include <vulkan/vulkan_raii.hpp>
#include <string>
#include <vector>

typedef struct VmaAllocator_T* VmaAllocator;
typedef struct VmaAllocation_T* VmaAllocation;

namespace crf {

class Texture {
public:
    Texture() = default;
    ~Texture();
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&&) noexcept;
    Texture& operator=(Texture&&) noexcept;

    bool loadFromFile(const std::string& filename, vk::raii::Device& device, VmaAllocator allocator,
                      vk::raii::CommandPool& pool, vk::raii::Queue& queue);
    void destroy(VkDevice device, VmaAllocator allocator);

    vk::Image        getImage()    const { return m_image; }
    vk::ImageView    getImageView()    const { return *m_imageView; }
    vk::Sampler      getSampler()      const { return *m_sampler; }
    uint32_t         getMipLevels()    const { return m_mipLevels; }
    uint32_t         getWidth()  const { return m_width; }
    uint32_t         getHeight() const { return m_height; }
    bool             isValid() const { return m_image != VK_NULL_HANDLE; }

private:
    VkImage             m_image = VK_NULL_HANDLE;
    VmaAllocation       m_allocation = nullptr;
    vk::raii::ImageView m_imageView = nullptr;
    vk::raii::Sampler   m_sampler = nullptr;
    uint32_t            m_mipLevels = 1;
    uint32_t            m_width = 0;
    uint32_t            m_height = 0;
};

} // namespace crf
