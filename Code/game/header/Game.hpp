#pragma once

#include "Character.hpp"
#include "SpriteSystem.hpp"
#include "Terrain.hpp"
#include <graphics/VulkanContext.hpp>
#include <graphics/VulkanRenderPass.hpp>

namespace crf {

class Window;

}

namespace game {

class Game {
public:
    Game(crf::VulkanContext& context, crf::VulkanRenderPass& renderPass, crf::Window& window);
    ~Game() = default;

    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;

    void init();
    void update(float dt);
    void render(VkCommandBuffer cmd, crf::u32 imageIndex, const float* viewPtr, const float* projPtr);

    float getCharX() const { return m_character.getX(); }
    float getCharY() const { return m_character.getY(); }
    float getCharZ() const { return m_character.getZ(); }

private:
    crf::VulkanContext& m_context;
    crf::VulkanRenderPass& m_renderPass;
    crf::Window& m_window;

    Character m_character;
    SpriteSystem m_spriteSystem;
};

}
