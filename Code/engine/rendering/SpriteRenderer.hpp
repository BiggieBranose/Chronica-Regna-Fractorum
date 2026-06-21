#pragma once
#include <vulkan/vulkan_raii.hpp>
#include <glm/glm.hpp>
#include <vector>
#include <array>
#include <cstdint>

typedef struct VmaAllocator_T* VmaAllocator;
typedef struct VmaAllocation_T* VmaAllocation;

namespace crf {

class Camera;
class Texture;

struct SpriteVertex {
    glm::vec3 position;
    glm::vec2 uv;
    glm::vec4 color;
};

struct SpritePushConstants {
    glm::mat4 mvp;
};

class SpriteRenderer {
public:
    SpriteRenderer() = default;
    ~SpriteRenderer();

    bool initialize(vk::raii::Device& device, VmaAllocator allocator,
                    vk::raii::CommandPool& pool, vk::raii::Queue& queue,
                    vk::Format colorFormat);
    void shutdown(VkDevice device, VmaAllocator allocator);

    void beginFrame(vk::raii::CommandBuffer& cmd, const Camera& camera);
    void draw(const Texture& texture, glm::vec2 position, glm::vec2 size,
              glm::vec4 color = glm::vec4(1.0f), float rotation = 0.0f);
    void endFrame();

    vk::raii::Pipeline&       getPipeline()       { return m_pipeline; }
    vk::raii::PipelineLayout& getPipelineLayout() { return m_pipelineLayout; }

private:
    bool createPipeline(vk::raii::Device& device, vk::Format colorFormat);
    bool createBuffers(VkDevice device, VmaAllocator allocator, vk::raii::CommandPool& pool, vk::raii::Queue& queue);

    struct SpriteBatch {
        const Texture* texture;
        glm::mat4 mvp;
    };

    std::vector<SpriteBatch> m_batches;

    vk::raii::PipelineLayout m_pipelineLayout = nullptr;
    vk::raii::Pipeline m_pipeline = nullptr;
    vk::raii::DescriptorSetLayout m_descriptorSetLayout = nullptr;
    vk::raii::DescriptorPool m_descriptorPool = nullptr;
    std::vector<vk::raii::DescriptorSet> m_descriptorSets;

    VkBuffer m_vertexBuffer = VK_NULL_HANDLE;
    VmaAllocation m_vertexAllocation = nullptr;
    VkBuffer m_indexBuffer = VK_NULL_HANDLE;
    VmaAllocation m_indexAllocation = nullptr;

    vk::Format m_colorFormat{};
    uint32_t m_currentFrame = 0;
    const Camera* m_currentCamera = nullptr;
    vk::raii::CommandBuffer* m_currentCmd = nullptr;
};

} // namespace crf
