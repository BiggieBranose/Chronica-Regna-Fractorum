#include "ImguiVulkanUtil.hpp"
#include "vulkan/vulkan.hpp"
#include <cstdint>
#include <imgui/imgui.h>
#include <vulkan/vulkan_core.h>

namespace crf{
ImGuiVulkanUtil::ImGuiVulkanUtil(vk::raii::Device& device, vk::raii::PhysicalDevice& physicalDevice,
        vk::raii::Queue& graphicsQueue, uint32_t graphicsQueueFamily)
        :   device(&device), physicalDevice(&physicalDevice),
            graphicsQueue(&graphicsQueue), graphicsQueueFamily(graphicsQueueFamily),

            vertexBuffer(*device, 1,
                        vk::BufferUsageFlagBits::eVertexBuffer,
                        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent),
            indexBuffer(*device, 1,
                        vk::BufferUsageFlagBits::eIndexBuffer,
                        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)   {
    
    renderingInfo.colorAttatchmentCount = 1;
    vk::Format formats[] = {colorFormat};
    renderingInfo.pColorAttatchmentFormats = &colorFormat;
}

ImGuiVulkanUtil::~ImGuiVulkanUtil(){

    if (device){
        device->waitIdle();
    }
}

void ImGuiVulkanUtil::init(float width, float height){
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;

    io.DisplaySize = ImVec2(width, height);
    io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);

    vulkanStyle = ImGui::GetStyle();
    vulkanStyle.Colors[ImGuiCol_TitleBg] = ImVec4(1.0f, 0.0f, 0.0f, 0.6f);
    vulkanStyle.Colors[ImGuiCol_TitleBgActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);
    vulkanStyle.Colors[ImGuiCol_MenuBarBg] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
    vulkanStyle.Colors[ImGuiCol_Header] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
    vulkanStyle.Colors[ImGuiCol_CheckMark] = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);

    setStyle(0);
}

void ImGuiVulkanUtil::setStyle(uint32_t index){
    ImGuiStyle& style = ImGui::GetStyle();

    switch (index) {
        case 0: // Custom Vulkan style
            style = vulkanStyle;
            break;
        case 1: // Classic style
            ImGui::StyleColorsClassic();
            break;
        case 2: // Dark style
            ImGui::StyleColorsDark();
            break;
        case 3: // Light style
            ImGui::StyleColorsLight();
            break;
    }
}

void ImGuiVulkanUtil::initResources(){
    vk::SamplerCreateInfo samplerInfo{};
    samplerInfo.magFilter = vk::Filter::eLinear;
    samplerInfo.minFilter = vk::Filter::eLinear;
    samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
    samplerInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.addressModeW = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.borderColor = vk::BorderColor::eFloatOpaqueWhite;

    sampler = device->createSampler(samplerInfo);

    vk::DescriptorPoolSize poolSize{vk::DescriptorType::eCombinedImageSampler, 1};

    vk::DescriptorPoolCreateInfo poolInfo{};
    poolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    poolInfo.maxSets = 2;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;

    descriptorPool = device->createDescriptorPool(poolInfo);

    vk::DescriptorSetAllocateInfo allocInfo{};
    allocInfo.descriptorPool = *descriptorPool;
    allocInfo.descriptorSetCount = 1;
    vk::DescriptorSetLayout layouts[] = {*descriptorSetLayout};
    allocInfo.pSetLayouts = layouts;

    descriptorSet = std::move(device->allocateDescriptorSets(allocInfo).front());

    vk::DescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    imageInfo.imageView = fontImageView.getHandle();
    imageInfo.sampler = *sampler;

    vk::WriteDescriptorSet writeSet{};
    writeSet.dstSet = *descriptorSet;
    writeSet.descriptorCount = 1;
    writeSet.descriptorType = vk::DescriptorType::eCombinedImageSampler;
    writeSet.pImageInfo = &imageInfo;
    writeSet.dstBinding = 0;

    device->updateDescriptorSets(1, &writeSet, 0, nullptr);

    vk::PipelineCacheCreateInfo pipelineCacheInfo{};
    pipelineCache = device->createPipelineCache(pipelineCacheInfo);

    vk::PushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = vk::ShaderStageFlagBits::eVertex;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(pushConstBlock);

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.setLayoutCount = 1;
    vk::DescriptorSetLayout setLayouts[] = {*descriptorSetLayout};
    pipelineLayoutInfo.pSetLayouts = setLayouts;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    pipelineLayout = device->createPipelineLayout(pipelineLayoutInfo);
}

void ImGuiVulkanUtil::updateTexture(ImTextureData *tex){
    if(tex->Status == ImTextureStatus_WantCreate || tex->Status == ImTextureStatus_WantUpdates){
        int texWidth = tex->Width;
        int texHeight = tex->Height;
        unsigned char* fontData = (unsigned char*)tex->Pixels;

        if(!fontData) return;

        vk::DeviceSize uploadSize = texWidth * texHeight * tex->BytesPerPixel;
        vk::Format format = (tex->BytesPerPixel == 4) ? vk::Format::eR8G8B8A8Unorm : vk::Format::eR8Unorm;

        if(tex->Status == ImTextureStatus_WantCreate){
            vk::Extent3D extent{static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), 1};
            fontImage = Image(*device, extent, format,
                            vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
                            vk::MemoryPropertyFlagBits::eDeviceLocal);
            fontImageView = ImageView(*device, fontImage.getHandle(), format,
                                    vk::ImageAspectFlagBits::eColor);
        }

        VkBuffer stagingBuffer(*device, uploadSize, vk::BufferUsageFlagBits::eTransferSrc,
                            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        
        void* data = stagingBuffer.map();
        memcpy(data, fontData, uploadSize);
        stagingBuffer.unmap();

        transitionImageLayout(fontImage.getHandle(), format,
                            vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
        copyBufferToImage(stagingBuffer.getHandle(), fontImage.getHandle(),
                        static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
        transitionImageLayout(fontImage.getHandle(), format,
                            vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
        
        tex->SetTexID((ImTextureID)(intptr_t)(VkDescriptorSet)*descriptorSet);
        tex->SetStatus(ImTextureStatus_OK);
    }
}
};
