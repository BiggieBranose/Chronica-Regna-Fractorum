#include "../header/Game.hpp"

namespace game {

Game::Game(crf::VulkanContext& context, crf::VulkanRenderPass& renderPass)
    : m_context(context), m_renderPass(renderPass) {
}

Game::~Game() {}

void Game::init() {
    // Initialize game resources here
}

void Game::update(float dt) {
    // Update game logic here
}

void Game::render(VkCommandBuffer cmd, crf::u32 imageIndex) {
    // Record rendering commands here
}

}