#pragma once

#include <graphics/VulkanContext.hpp>
#include <graphics/VulkanRenderPass.hpp>
#include <graphics/VulkanPipeline.hpp>
#include <graphics/VulkanBuffer.hpp>

namespace game {

class Game {
public:
    Game(crf::VulkanContext& context, crf::VulkanRenderPass& renderPass);
    ~Game();

    void init();

    void update(float dt);
    void render(VkCommandBuffer cmd, crf::u32 imageIndex);

private:
    crf::VulkanContext& m_context;
    crf::VulkanRenderPass& m_renderPass;
};

}
