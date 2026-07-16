#pragma once

#include "core/Types.hpp"
#include "graphics/VulkanContext.hpp"
#include "render/BindlessManager.hpp"
#include "render/RenderGraph.hpp"
#include "render/ShadowMap.hpp"
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include <glm/glm.hpp>

namespace crf {

class SpriteRenderer {
public:
    struct Sprite {
        glm::vec3 position = {0, 0, 0};
        glm::vec2 scale = {1, 1};
        float rotation = 0.0f;
        uint32_t textureIndex = 0;
        glm::vec4 color = {1, 1, 1, 1};
        bool flipX = false;
        bool flipY = false;
        int layer = 0;
    };

    struct DrawCommand {
        uint32_t textureIndex = 0;
        uint32_t firstSprite = 0;
        uint32_t spriteCount = 0;
    };

    SpriteRenderer(VulkanContext& context, BindlessDescriptorManager& bindless);
    ~SpriteRenderer();

    void init(RenderGraph& renderGraph, VkFormat colorFormat, VkFormat depthFormat);
    void cleanup();

    void beginFrame();
    void addSprite(const Sprite& sprite);
    void endFrame();

    void recordCommands(VkCommandBuffer cmd, const RenderGraph& graph, const std::string& colorImage, const std::string& depthImage);

    uint32_t getSpriteCount() const { return m_sprites.size(); }
    void clear() { m_sprites.clear(); }

private:
    VulkanContext& m_context;
    BindlessDescriptorManager& m_bindless;

    struct Vertex {
        glm::vec2 pos;
        glm::vec2 uv;
        uint32_t textureIndex;
        uint32_t pad;
    };

    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_descSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet m_descSet = VK_NULL_HANDLE;

    std::vector<Sprite> m_sprites;
    std::vector<Vertex> m_vertices;
    std::vector<uint32_t> m_indices;

    VkBuffer m_vertexBuffer = VK_NULL_HANDLE;
    VmaAllocation m_vertexAlloc = VK_NULL_HANDLE;
    VkBuffer m_indexBuffer = VK_NULL_HANDLE;
    VmaAllocation m_indexAlloc = VK_NULL_HANDLE;
    VkBuffer m_indirectBuffer = VK_NULL_HANDLE;
    VmaAllocation m_indirectAlloc = VK_NULL_HANDLE;

    uint32_t m_maxSprites = 10000;
    uint32_t m_spriteCount = 0;
    std::vector<DrawCommand> m_drawCommands;

    void createPipeline(VkFormat colorFormat, VkFormat depthFormat);
    void createBuffers();
    void updateBuffers();
    void sortSprites();
    void buildDrawCommands();
};

}