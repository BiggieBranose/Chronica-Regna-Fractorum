#include "SpriteRenderer.hpp"
#include "core/Log.hpp"
#include "core/Assert.hpp"
#include "graphics/VulkanContext.hpp"
#include "graphics/VulkanPipeline.hpp"
#include "graphics/VulkanBuffer.hpp"
#include <vulkan/vulkan.h>
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace crf {

SpriteRenderer::SpriteRenderer(VulkanContext& context, BindlessDescriptorManager& bindless)
    : m_context(context), m_bindless(bindless) {}

SpriteRenderer::~SpriteRenderer() {
    cleanup();
}

void SpriteRenderer::init(RenderGraph& renderGraph, VkFormat colorFormat, VkFormat depthFormat) {
    createPipeline(colorFormat, depthFormat);
    createBuffers();
    Log::info("SpriteRenderer initialized");
}

void SpriteRenderer::cleanup() {
    VkDevice device = m_context.getDevice();
    if (m_indirectBuffer) {
        vmaDestroyBuffer(m_context.getAllocator(), m_indirectBuffer, m_indirectAlloc);
    }
    if (m_indexBuffer) {
        vmaDestroyBuffer(m_context.getAllocator(), m_indexBuffer, m_indexAlloc);
    }
    if (m_vertexBuffer) {
        vmaDestroyBuffer(m_context.getAllocator(), m_vertexBuffer, m_vertexAlloc);
    }
    if (m_pipeline) vkDestroyPipeline(m_context.getDevice(), m_pipeline, nullptr);
    if (m_pipelineLayout) vkDestroyPipelineLayout(m_context.getDevice(), m_pipelineLayout, nullptr);
    m_sprites.clear();
    m_vertices.clear();
    m_indices.clear();
    m_drawCommands.clear();
}

void SpriteRenderer::beginFrame() {
    m_spriteCount = 0;
    m_sprites.clear();
}

void SpriteRenderer::addSprite(const Sprite& sprite) {
    if (m_spriteCount >= m_maxSprites) return;
    m_sprites.push_back(sprite);
    m_spriteCount++;
}

void SpriteRenderer::endFrame() {
    sortSprites();
    buildDrawCommands();
    updateBuffers();
}

void SpriteRenderer::sortSprites() {
    std::sort(m_sprites.begin(), m_sprites.end(), [](const Sprite& a, const Sprite& b) {
        if (a.layer != b.layer) return a.layer < b.layer;
        if (a.textureIndex != b.textureIndex) return a.textureIndex < b.textureIndex;
        return a.position.z < b.position.z;
    });
}

void SpriteRenderer::buildDrawCommands() {
    m_vertices.clear();
    m_indices.clear();
    m_drawCommands.clear();

    m_vertices.reserve(m_spriteCount * 4);
    m_indices.reserve(m_spriteCount * 6);

    uint32_t vertexOffset = 0;
    uint32_t indexOffset = 0;

    for (const auto& sprite : m_sprites) {
        uint32_t startVertex = m_vertices.size();
        uint32_t startIndex = m_indices.size();

        float halfW = sprite.scale.x * 0.5f;
        float halfH = sprite.scale.y * 0.5f;
        float u0 = sprite.flipX ? 1.0f : 0.0f;
        float u1 = sprite.flipX ? 0.0f : 1.0f;
        float v0 = sprite.flipY ? 1.0f : 0.0f;
        float v1 = sprite.flipY ? 0.0f : 1.0f;

        float cosR = std::cos(sprite.rotation);
        float sinR = std::sin(sprite.rotation);

        auto rot = [&](float x, float y) {
            return glm::vec2(cosR * x - sinR * y, sinR * x + cosR * y);
        };

        // 4 vertices: BL, BR, TR, TL
        m_vertices.push_back({rot(-halfW, -halfH) + glm::vec2(sprite.position.x, sprite.position.y), {u0, v0}, sprite.textureIndex, 0});
        m_vertices.push_back({rot( halfW, -halfH) + glm::vec2(sprite.position.x, sprite.position.y), {u1, v0}, sprite.textureIndex, 0});
        m_vertices.push_back({rot( halfW,  halfH) + glm::vec2(sprite.position.x, sprite.position.y), {u1, v1}, sprite.textureIndex, 0});
        m_vertices.push_back({rot(-halfW,  halfH) + glm::vec2(sprite.position.x, sprite.position.y), {u0, v1}, sprite.textureIndex, 0});

        m_indices.push_back(vertexOffset + 0);
        m_indices.push_back(vertexOffset + 1);
        m_indices.push_back(vertexOffset + 2);
        m_indices.push_back(vertexOffset + 2);
        m_indices.push_back(vertexOffset + 3);
        m_indices.push_back(vertexOffset + 0);

        vertexOffset += 4;
        indexOffset += 6;

        if (m_drawCommands.empty() || 
            m_drawCommands.back().textureIndex != sprite.textureIndex) {
            DrawCommand dc;
            dc.textureIndex = sprite.textureIndex;
            dc.firstSprite = m_drawCommands.size();
            dc.spriteCount = 1;
            m_drawCommands.push_back(dc);
        } else {
            m_drawCommands.back().spriteCount++;
        }
    }
}

void SpriteRenderer::updateBuffers() {
    if (m_vertices.empty()) return;

    VkDeviceSize vertexSize = m_vertices.size() * sizeof(Vertex);
    VkDeviceSize indexSize = m_indices.size() * sizeof(uint32_t);

    VkBufferCreateInfo bufInfo{};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufInfo.size = vertexSize;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    if (m_vertexBuffer) {
        vmaDestroyBuffer(m_context.getAllocator(), m_vertexBuffer, m_vertexAlloc);
        vmaDestroyBuffer(m_context.getAllocator(), m_indexBuffer, m_indexAlloc);
    }

    VkResult result = vmaCreateBuffer(m_context.getAllocator(), &bufInfo, &allocInfo{}, &m_vertexBuffer, &m_vertexAlloc, nullptr);
    CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to create vertex buffer");

    bufInfo.size = indexSize;
    bufInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    result = vmaCreateBuffer(m_context.getAllocator(), &bufInfo, &allocInfo{}, &m_indexBuffer, &m_indexAlloc, nullptr);
    CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to create index buffer");

    // Staging buffers
    VkBuffer stagingVert, stagingIdx;
    VmaAllocation allocVert, allocIdx;
    VkBufferCreateInfo stagingInfo{};
    stagingInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingInfo.size = vertexSize;
    stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo stagingAlloc{};
    stagingAlloc.usage = VMA_MEMORY_USAGE_CPU_ONLY;

    vmaCreateBuffer(m_context.getAllocator(), &bufInfo, &stagingAlloc, &stagingVert, &allocVert, nullptr);
    void* data;
    vmaMapMemory(m_context.getAllocator(), allocVert, &data);
    std::memcpy(data, m_vertices.data(), vertexSize);
    vmaUnmapMemory(m_context.getAllocator(), allocVert);

    bufInfo.size = indexSize;
    vmaCreateBuffer(m_context.getAllocator(), &bufInfo, &stagingAlloc, &stagingIdx, &allocIdx, nullptr);
    vmaMapMemory(m_context.getAllocator(), allocIdx, &data);
    std::memcpy(data, m_indices.data(), indexSize);
    vmaUnmapMemory(m_context.getAllocator(), allocIdx);

    VkCommandBuffer cmd = m_context.getGraphicsQueue() == m_context.getPresentQueue() 
        ? m_context.getCommandBuffer(0) : VK_NULL_HANDLE;
    // Use single-time commands for transfer
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_context.getCommandPool();
    allocInfo.commandBufferCount = 1;
    VkCommandBuffer transferCmd;
    vkAllocateCommandBuffers(m_context.getDevice(), &allocInfo, &transferCmd);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(transferCmd, &beginInfo);

    VkBufferCopy vertCopy{};
    vertCopy.size = vertexSize;
    vkCmdCopyBuffer(transferCmd, stagingVert, m_vertexBuffer, 1, &vertCopy);

    VkBufferCopy idxCopy{};
    idxCopy.size = indexSize;
    vkCmdCopyBuffer(transferCmd, stagingIdx, m_indexBuffer, 1, &idxCopy);

    vkEndCommandBuffer(transferCmd);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &transferCmd;
    vkQueueSubmit(m_context.getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_context.getGraphicsQueue());

    vkFreeCommandBuffers(m_context.getDevice(), m_context.getCommandPool(), 1, &transferCmd);
    vmaDestroyBuffer(m_context.getAllocator(), stagingVert, allocVert);
    vmaDestroyBuffer(m_context.getAllocator(), stagingIdx, allocIdx);
}

void SpriteRenderer::createPipeline(VkFormat colorFormat, VkFormat depthFormat) {
    VkDescriptorSetLayoutBinding bindings[] = {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
    };

    VkDescriptorSetLayoutCreateInfo dslInfo{};
    dslInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dslInfo.bindingCount = 1;
    dslInfo.pBindings = &dslInfo;
    vkCreateDescriptorSetLayout(m_context.getDevice(), &dslInfo, nullptr, &m_descSetLayout);

    VkPushConstantRange pcRange{};
    pcRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pcRange.offset = 0;
    pcRange.size = sizeof(glm::mat4) * 2;

    VkPipelineLayoutCreateInfo plInfo{};
    plInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plInfo.setLayoutCount = 1;
    plInfo.pSetLayouts = &m_descSetLayout;
    plInfo.pushConstantRangeCount = 1;
    plInfo.pPushConstantRanges = &pcRange;
    vkCreatePipelineLayout(m_context.getDevice(), &pcInfo, nullptr, &m_pipelineLayout);

    auto vertCode = crf::VulkanPipeline::readFile("shaders/sprite.vert.spv");
    auto fragCode = crf::VulkanPipeline::readFile("shaders/sprite.frag.spv");
    VkShaderModule vertModule = crf::VulkanPipeline::createShaderModule(m_context.getDevice(), vertCode);
    VkShaderModule fragModule = crf::VulkanPipeline::createShaderModule(m_context.getDevice(), fragCode);

    VkPipelineShaderStageCreateInfo stages[2] = {
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vertModule, "main", nullptr},
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fragModule, "main", nullptr},
    };

    VkVertexInputBindingDescription viBinding{};
    viBinding.binding = 0;
    viBinding.stride = sizeof(Vertex);
    viBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription viAttrs[4] = {
        {0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, pos)},
        {1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv)},
        {2, 0, VK_FORMAT_R32_UINT, offsetof(Vertex, textureIndex)},
    };

    VkPipelineVertexInputStateCreateInfo viInfo{};
    viInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    viInfo.vertexBindingDescriptionCount = 1;
    viInfo.pVertexBindingDescriptions = &viBinding;
    viInfo.vertexAttributeDescriptionCount = 3;
    viInfo.pVertexAttributeDescriptions = viAttrs;

    VkPipelineInputAssemblyStateCreateInfo iaInfo{};
    iaInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    iaInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo vpInfo{};
    vpInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vpInfo.viewportCount = 1;
    vpInfo.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rastInfo{};
    rastInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rastInfo.polygonMode = VK_POLYGON_MODE_FILL;
    rastInfo.cullMode = VK_CULL_MODE_BACK_BIT;
    rastInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rastInfo.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo msInfo{};
    msInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    msInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo dsInfo{};
    dsInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    dsInfo.depthTestEnable = VK_TRUE;
    dsInfo.depthWriteEnable = VK_TRUE;
    dsInfo.depthCompareOp = VK_COMPARE_OP_LESS;
    dsInfo.depthBoundsTestEnable = VK_FALSE;
    dsInfo.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState cbAttach{};
    cbAttach.blendEnable = VK_TRUE;
    cbAttach.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    cbAttach.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    cbAttach.colorBlendOp = VK_BLEND_OP_ADD;
    cbAttach.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    cbAttach.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    cbAttach.alphaBlendOp = VK_BLEND_OP_ADD;
    cbAttach.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo cbInfo{};
    cbInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cbInfo.attachmentCount = 1;
    cbInfo.pAttachments = &cbAttach;

    VkDynamicState dynStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynInfo{};
    dynInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynInfo.dynamicStateCount = 2;
    dynInfo.pDynamicStates = dynStates;

    VkGraphicsPipelineCreateInfo pipeInfo{};
    pipeInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeInfo.stageCount = 2;
    pipeInfo.pStages = stages;
    pipeInfo.pVertexInputState = &viInfo;
    pipeInfo.pInputAssemblyState = &iaInfo;
    pipeInfo.pViewportState = &vpInfo;
    pipeInfo.pRasterizationState = &rastInfo;
    pipeInfo.pMultisampleState = &msInfo;
    pipeInfo.pDepthStencilState = &dsInfo;
    pipeInfo.pColorBlendState = &cbInfo;
    pipeInfo.pDynamicState = &dynInfo;
    pipeInfo.layout = m_pipelineLayout;
    pipeInfo.renderPass = VK_NULL_HANDLE; // Will be set by render graph
    pipeInfo.subpass = 0;

    VkResult result = vkCreateGraphicsPipelines(m_context.getDevice(), VK_NULL_HANDLE, 1, &pipeInfo, nullptr, &m_pipeline);
    CRF_ASSERT_MSG(result == VK_SUCCESS, "Failed to create sprite pipeline");

    vkDestroyShaderModule(m_context.getDevice(), vertModule, nullptr);
    vkDestroyShaderModule(m_context.getDevice(), fragModule, nullptr);

    Log::info("Sprite pipeline created");
}

void SpriteRenderer::createBuffers() {
    // Indirect draw buffer
    VkBufferCreateInfo bufInfo{};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size = sizeof(VkDrawIndexedIndirectCommand) * 1024;
    bufInfo.usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    vmaCreateBuffer(m_context.getAllocator(), &bufInfo, &allocInfo{}, &m_indirectBuffer, &m_indirectAlloc, nullptr);
}

void SpriteRenderer::recordCommands(VkCommandBuffer cmd, const RenderGraph& graph, const std::string& colorImage, const std::string& depthImage) {
    if (m_drawCommands.empty()) return;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

    VkViewport viewport{};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = 1280;
    viewport.height = 720;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = {1280, 720};
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    VkBuffer vertexBuffers[] = {m_vertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(cmd, m_indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descSet, 0, nullptr);

    for (const auto& dc : m_drawCommands) {
        if (dc.spriteCount == 0) continue;

        // Push constant: view-proj matrix
        // TODO: push view-proj per cascade or frame

        vkCmdDrawIndexedIndirect(cmd, m_indirectBuffer, 0, m_drawCommands.size(), sizeof(VkDrawIndexedIndirectCommand));
    }
}

}