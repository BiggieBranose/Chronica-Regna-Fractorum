#include "SpriteRenderer.hpp"
#include "Camera.hpp"
#include "Texture.hpp"
#include "../core/Log.hpp"
#include "../core/File.hpp"
#include "../../external/VMA/vk_mem_alloc.h"
#include <cstring>

namespace crf {

static VmaAllocator toVma(void* p) { return static_cast<VmaAllocator>(p); }

SpriteRenderer::~SpriteRenderer() {}

bool SpriteRenderer::initialize(vk::raii::Device& device, VmaAllocator allocator,
                                vk::raii::CommandPool& pool, vk::raii::Queue& queue,
                                vk::Format colorFormat)
{
    m_colorFormat = colorFormat;

    if (!createBuffers(*device, allocator, pool, queue)) return false;
    if (!createPipeline(device, colorFormat)) return false;

    Log::info("SpriteRenderer initialized");
    return true;
}

void SpriteRenderer::shutdown(VkDevice device, VmaAllocator allocator) {
    if (m_vertexBuffer) {
        vmaDestroyBuffer(toVma(allocator), m_vertexBuffer, m_vertexAllocation);
        m_vertexBuffer = VK_NULL_HANDLE;
    }
    if (m_indexBuffer) {
        vmaDestroyBuffer(toVma(allocator), m_indexBuffer, m_indexAllocation);
        m_indexBuffer = VK_NULL_HANDLE;
    }
    m_descriptorSets.clear();
    m_descriptorPool = nullptr;
    m_descriptorSetLayout = nullptr;
    m_pipeline = nullptr;
    m_pipelineLayout = nullptr;
    Log::info("SpriteRenderer shutdown");
}

bool SpriteRenderer::createBuffers(VkDevice device, VmaAllocator allocator,
                                   vk::raii::CommandPool& pool, vk::raii::Queue& queue)
{
    std::array<SpriteVertex, 4> vertices = {{
        {glm::vec3(-0.5f, -0.5f, 0.0f), glm::vec2(0.0f, 1.0f), glm::vec4(1.0f)},
        {glm::vec3( 0.5f, -0.5f, 0.0f), glm::vec2(1.0f, 1.0f), glm::vec4(1.0f)},
        {glm::vec3( 0.5f,  0.5f, 0.0f), glm::vec2(1.0f, 0.0f), glm::vec4(1.0f)},
        {glm::vec3(-0.5f,  0.5f, 0.0f), glm::vec2(0.0f, 0.0f), glm::vec4(1.0f)},
    }};
    std::array<uint16_t, 6> indices = {0, 1, 2, 2, 3, 0};

    VkBufferCreateInfo vbInfo{};
    vbInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vbInfo.size = sizeof(vertices);
    vbInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    vbInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                      VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VmaAllocationInfo allocOut;
    if (vmaCreateBuffer(toVma(allocator), &vbInfo, &allocInfo,
            &m_vertexBuffer, &m_vertexAllocation, &allocOut) != VK_SUCCESS) {
        Log::error("Failed to create sprite vertex buffer");
        return false;
    }
    memcpy(allocOut.pMappedData, vertices.data(), sizeof(vertices));

    VkBufferCreateInfo ibInfo = vbInfo;
    ibInfo.size = sizeof(indices);
    ibInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

    if (vmaCreateBuffer(toVma(allocator), &ibInfo, &allocInfo,
            &m_indexBuffer, &m_indexAllocation, &allocOut) != VK_SUCCESS) {
        Log::error("Failed to create sprite index buffer");
        return false;
    }
    memcpy(allocOut.pMappedData, indices.data(), sizeof(indices));

    return true;
}

bool SpriteRenderer::createPipeline(vk::raii::Device& device, vk::Format colorFormat) {
    auto shaderCode = File::readBinary("shaders/graphics.spv");
    if (!shaderCode) {
        Log::error("Failed to read sprite shader");
        return false;
    }
    const uint32_t* code = reinterpret_cast<const uint32_t*>(shaderCode->data());
    size_t codeSize = shaderCode->size();

    vk::ShaderModuleCreateInfo smInfo{};
    smInfo.codeSize = codeSize;
    smInfo.pCode = code;
    vk::raii::ShaderModule vertModule(device, smInfo);
    vk::raii::ShaderModule fragModule(device, smInfo);

    std::array<vk::PipelineShaderStageCreateInfo, 2> stages{};
    stages[0].stage = vk::ShaderStageFlagBits::eVertex;
    stages[0].module = *vertModule;
    stages[0].pName = "vertMain";
    stages[1].stage = vk::ShaderStageFlagBits::eFragment;
    stages[1].module = *fragModule;
    stages[1].pName = "fragMain";

    auto bindingDesc = vk::VertexInputBindingDescription(0, sizeof(SpriteVertex), vk::VertexInputRate::eVertex);
    std::array<vk::VertexInputAttributeDescription, 3> attrs{};
    attrs[0] = vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(SpriteVertex, position));
    attrs[1] = vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32Sfloat, offsetof(SpriteVertex, uv));
    attrs[2] = vk::VertexInputAttributeDescription(2, 0, vk::Format::eR32G32B32A32Sfloat, offsetof(SpriteVertex, color));

    vk::PipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &bindingDesc;
    vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrs.size());
    vertexInput.pVertexAttributeDescriptions = attrs.data();

    vk::PipelineInputAssemblyStateCreateInfo inputAsm{};
    inputAsm.topology = vk::PrimitiveTopology::eTriangleList;

    vk::PipelineViewportStateCreateInfo viewport{};
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;

    vk::PipelineRasterizationStateCreateInfo raster{};
    raster.polygonMode = vk::PolygonMode::eFill;
    raster.cullMode = vk::CullModeFlagBits::eBack;
    raster.frontFace = vk::FrontFace::eClockwise;
    raster.lineWidth = 1.0f;

    vk::PipelineMultisampleStateCreateInfo ms{};
    ms.rasterizationSamples = vk::SampleCountFlagBits::e1;

    vk::PipelineColorBlendAttachmentState blend{};
    blend.blendEnable = VK_TRUE;
    blend.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
    blend.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
    blend.colorBlendOp = vk::BlendOp::eAdd;
    blend.srcAlphaBlendFactor = vk::BlendFactor::eOne;
    blend.dstAlphaBlendFactor = vk::BlendFactor::eZero;
    blend.alphaBlendOp = vk::BlendOp::eAdd;
    blend.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                           vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

    vk::PipelineColorBlendStateCreateInfo blendState{};
    blendState.attachmentCount = 1;
    blendState.pAttachments = &blend;

    std::array<vk::DynamicState, 2> dynStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
    vk::PipelineDynamicStateCreateInfo dyn{};
    dyn.dynamicStateCount = static_cast<uint32_t>(dynStates.size());
    dyn.pDynamicStates = dynStates.data();

    vk::PushConstantRange pushConst{};
    pushConst.stageFlags = vk::ShaderStageFlagBits::eVertex;
    pushConst.offset = 0;
    pushConst.size = sizeof(glm::mat4);

    vk::PipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushConst;
    m_pipelineLayout = vk::raii::PipelineLayout(device, layoutInfo);

    vk::GraphicsPipelineCreateInfo pipe{};
    pipe.stageCount = static_cast<uint32_t>(stages.size());
    pipe.pStages = stages.data();
    pipe.pVertexInputState = &vertexInput;
    pipe.pInputAssemblyState = &inputAsm;
    pipe.pViewportState = &viewport;
    pipe.pRasterizationState = &raster;
    pipe.pMultisampleState = &ms;
    pipe.pColorBlendState = &blendState;
    pipe.pDynamicState = &dyn;
    pipe.layout = *m_pipelineLayout;
    pipe.renderPass = nullptr;
    pipe.subpass = 0;

    vk::PipelineRenderingCreateInfo renderInfo{};
    renderInfo.colorAttachmentCount = 1;
    renderInfo.pColorAttachmentFormats = &colorFormat;
    vk::StructureChain chain(pipe, renderInfo);
    m_pipeline = vk::raii::Pipeline(device, nullptr, chain.get<vk::GraphicsPipelineCreateInfo>());

    return true;
}

void SpriteRenderer::beginFrame(vk::raii::CommandBuffer& cmd, const Camera& camera) {
    m_currentCmd = &cmd;
    m_currentCamera = &camera;
    m_batches.clear();
}

void SpriteRenderer::draw(const Texture& texture, glm::vec2 position, glm::vec2 size,
                          glm::vec4 color, float rotation)
{
    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(position, 0.0f));
    model = glm::rotate(model, glm::radians(rotation), glm::vec3(0.0f, 0.0f, 1.0f));
    model = glm::scale(model, glm::vec3(size, 1.0f));
    glm::mat4 mvp = m_currentCamera->getViewProjection() * model;
    m_batches.push_back({&texture, mvp});
}

void SpriteRenderer::endFrame() {
    if (!m_currentCmd || m_batches.empty()) return;

    auto& cmd = *m_currentCmd;
    vk::Viewport vp{};
    vp.x = 0; vp.y = 0;
    vp.width = 800; vp.height = 600;
    vp.minDepth = 0; vp.maxDepth = 1;
    cmd.setViewport(0, vp);

    vk::Rect2D scissor({0, 0}, {800, 600});
    cmd.setScissor(0, scissor);

    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *m_pipeline);
    cmd.bindVertexBuffers(0, {vk::Buffer(m_vertexBuffer)}, {vk::DeviceSize(0)});
    cmd.bindIndexBuffer(vk::Buffer(m_indexBuffer), vk::DeviceSize(0), vk::IndexType::eUint16);

    for (auto& batch : m_batches) {
        cmd.pushConstants<glm::mat4>(*m_pipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, batch.mvp);
        cmd.drawIndexed(6, 1, 0, 0, 0);
    }

    m_currentCmd = nullptr;
    m_currentCamera = nullptr;
}

} // namespace crf
