#include "../header/Game.hpp"
#include <graphics/Window.hpp>
#include <core/Log.hpp>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace game {

Game::Game(crf::VulkanContext& context, crf::VulkanRenderPass& renderPass, crf::Window& window)
    : m_context(context), m_renderPass(renderPass), m_window(window),
      m_spriteSystem(context, renderPass) {
}

void Game::init() {
    crf::Log::info("Game: initializing");
    m_spriteSystem.init();
    crf::Log::info("Game: ready");
}

void Game::update(float dt) {
    m_character.update(dt,
        m_window.isKeyPressed(GLFW_KEY_W),
        m_window.isKeyPressed(GLFW_KEY_A),
        m_window.isKeyPressed(GLFW_KEY_S),
        m_window.isKeyPressed(GLFW_KEY_D),
        terrainHeight);
}

void Game::render(VkCommandBuffer cmd, crf::u32 imageIndex, const float* viewPtr, const float* projPtr) {
    float ty = terrainHeight(m_character.getX(), m_character.getZ());
    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(m_character.getX(), ty + 0.5f, m_character.getZ()));
    model = glm::scale(model, glm::vec3(0.5f, 0.8f, 1.0f));

    m_spriteSystem.render(cmd, imageIndex, glm::value_ptr(model), viewPtr, projPtr);
}

}
