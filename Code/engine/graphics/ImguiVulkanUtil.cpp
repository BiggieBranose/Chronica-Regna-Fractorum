#include "ImguiVulkanUtil.hpp"
#include "vulkan/vulkan.hpp"
#include <cstdint>
#include <cstring>
#include <glm/ext/vector_float2.hpp>
#include <imgui/imgui.h>
#include <sys/stat.h>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_raii.hpp>

namespace crf{
ImGuiVulkanUtil::ImGuiVulkanUtil(vk::raii::Device& device, vk::raii::PhysicalDevice& physicalDevice,
        vk::raii::Queue& graphicsQueue, uint32_t graphicsQueueFamily)
        // BUG: VkBuffer is a raw C handle (VkBuffer_T*), not a class - it cannot be constructed
        // with (device, size, usage, memoryProps). Intended: 1-byte placeholder GPU buffers here;
        // updateBuffers() reallocates them at the real ImGui vertex/index counts.
        :   vertexBuffer(*device, 1,
                        vk::BufferUsageFlagBits::eVertexBuffer,
                        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent),
            indexBuffer(*device, 1,
                        vk::BufferUsageFlagBits::eIndexBuffer,
                        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent),
            device(&device), physicalDevice(&physicalDevice),

            graphicsQueue(&graphicsQueue),
            graphicsQueueFamily(graphicsQueueFamily)   {
    
    // BUG: renderingInfo (the member) is configured here but never used - no pipeline is ever created,
    // so the format never reaches anything. The local `formats` array is dead. drawFrame() also declares
    // a local vk::RenderingInfo with the same name, shadowing this member.
    // Intended: tell the future graphics pipeline that ImGui renders to one B8G8R8A8 color attachment
    // via dynamic rendering (colorFormat - the swapchain color format).
    renderingInfo.colorAttachmentCount = 1;
    vk::Format formats[] = {colorFormat};
    renderingInfo.pColorAttachmentFormats = &colorFormat;
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

    // BUG: descriptorSetLayout was never created anywhere (it stays {nullptr}), so the set is allocated
    // against VK_NULL_HANDLE. Intended: descriptorSetLayout should describe binding 0 = combined image
    // sampler (the font texture).
    //   descriptorSet - the one descriptor set holding the font atlas sampler/image binding.
    vk::DescriptorSetAllocateInfo allocInfo{};
    allocInfo.descriptorPool = *descriptorPool;
    allocInfo.descriptorSetCount = 1;
    vk::DescriptorSetLayout layouts[] = {*descriptorSetLayout};
    allocInfo.pSetLayouts = layouts;

    descriptorSet = std::move(device->allocateDescriptorSets(allocInfo).front());

    // BUG: fontImageView is a raw handle (no getHandle()) and is not created yet - updateTexture()
    // creates it later, so this descriptor write binds a null image view.
    // Intended: bind the font atlas image view + sampler (sampler - linear/clamped sampler created
    // above) into descriptorSet at binding 0.
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

    // BUG: same null descriptorSetLayout as above, so the pipeline layout is built against VK_NULL_HANDLE.
    // Intended: layout exposing binding 0 (font sampler) + one vertex-stage push constant range
    // (pushConstBlock: scale/translate for pixel->NDC conversion).
    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.setLayoutCount = 1;
    vk::DescriptorSetLayout setLayouts[] = {*descriptorSetLayout};
    pipelineLayoutInfo.pSetLayouts = setLayouts;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    pipelineLayout = device->createPipelineLayout(pipelineLayoutInfo);

    // BUG: the graphics pipeline is never created here - no createGraphicsPipeline() call, no vertex/
    // fragment shader modules. `pipeline` (and the unused `pipelineCache`) stay null; drawFrame() then
    // binds *pipeline == VK_NULL_HANDLE. Intended: build the ImGui pipeline using renderingInfo (color
    // format), pipelineLayout, and ImGui's shaders.
}

void ImGuiVulkanUtil::updateTexture(ImTextureData *tex){
    if(tex->Status == ImTextureStatus_WantCreate || tex->Status == ImTextureStatus_WantUpdates){
        int texWidth = tex->Width;
        int texHeight = tex->Height;
        unsigned char* fontData = (unsigned char*)tex->Pixels;

        if(!fontData) return;

        vk::DeviceSize uploadSize = texWidth * texHeight * tex->BytesPerPixel;
        vk::Format format = (tex->BytesPerPixel == 4) ? vk::Format::eR8G8B8A8Unorm : vk::Format::eR8Unorm;

        // BUG: Image()/ImageView() don't exist in the project, and fontImage is a raw VkImage handle
        // (no .getHandle()). Intended: create the GPU font atlas image (device-local, sampled +
        // transfer-dst, format chosen from BytesPerPixel) and its color image view.
        //   texWidth/texHeight - atlas dimensions in pixels; fontData - CPU pixel pointer from ImTextureData.
        if(tex->Status == ImTextureStatus_WantCreate){
            vk::Extent3D extent{static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), 1};
            fontImage = Image(*device, extent, format,
                            vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
                            vk::MemoryPropertyFlagBits::eDeviceLocal);
            fontImageView = ImageView(*device, fontImage.getHandle(), format,
                                    vk::ImageAspectFlagBits::eColor);
        }

        // BUG: VkBuffer is a raw C handle, not a class - no such constructor/map()/unmap() exist.
        // Intended: CPU-visible staging buffer (uploadSize = texWidth*texHeight*BytesPerPixel bytes)
        // to copy the atlas pixels (fontData) into before blitting to the GPU image.
        VkBuffer stagingBuffer(*device, uploadSize, vk::BufferUsageFlagBits::eTransferSrc,
                            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        
        void* data = stagingBuffer.map();
        memcpy(data, fontData, uploadSize);
        stagingBuffer.unmap();

        // BUG: transitionImageLayout()/copyBufferToImage() are free functions that don't exist (the
        // VulkanTexture versions are member functions with a mipLevels arg and need a command pool/queue,
        // which this class doesn't hold). Intended: Undefined -> TransferDstOptimal, copy staging ->
        // image, TransferDstOptimal -> ShaderReadOnlyOptimal so the font is sampleable in the shader.
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

bool ImGuiVulkanUtil::newFrame(){

    ImGui::NewFrame();

    // Example text here:
    ImGui::Begin("Dummy text"); 
    ImGui::Text("Hello World");
    // BUG: ImGui::Text() inside the button's if-branch only runs during the single frame the button
    // was clicked, so "Button clicked" flashes for one frame instead of persisting. This is demo
    // content; intended: show "Hello World", and a message after the button is pressed.
    if(ImGui::Button("I'm a button!")){
        ImGui::Text("Button clicked");
    }
    ImGui::End();

    ImGui::EndFrame();

    ImGui::Render();

    ImDrawData* drawData = ImGui::GetDrawData();
    if(drawData && drawData->CmdListsCount > 0){
        // BUG: needsUpdateBuffers is set here but never read anywhere - the flag is dead; only the
        // return value tells the caller. Intended: signal "geometry grew, re-upload buffers" when the
        // frame's total vertex/index counts exceed the current buffer capacity (vertexCount/indexCount).
        if(drawData->TotalVtxCount > vertexCount || drawData->TotalIdxCount > indexCount){
            needsUpdateBuffers = true;
            return true;
        }
    }
    return false;
}

void ImGuiVulkanUtil::updateBuffers(){
    ImDrawData* drawData = ImGui::GetDrawData();
    if(!drawData || drawData->CmdListsCount == 0){
        return;
    }

    vk::DeviceSize vertexBufferSize = drawData->TotalVtxCount * sizeof(ImDrawVert);
    vk::DeviceSize indexBufferSize = drawData->TotalIdxCount * sizeof(ImDrawIdx);

    // BUG: two problems. (1) VkBuffer is a raw C handle, not a class - no constructor/assignment.
    // (2) Memory flags use logical OR (||) instead of bitwise OR (|), collapsing the flags to a boolean
    // (same bug in both vertex and index branches). Intended: reallocate the vertex/index buffers when
    // the frame needs more capacity than vertexCount/indexCount, requesting eHostVisible AND eHostCoherent
    // so CPU writes are directly visible to the GPU.
    if(drawData->TotalVtxCount > vertexCount){
        vertexBuffer = VkBuffer(*device, vertexBufferSize,
                                vk::BufferUsageFlagBits::eVertexBuffer,
                                vk::MemoryPropertyFlagBits::eHostVisible || vk::MemoryPropertyFlagBits::eHostCoherent);
        vertexCount = drawData->TotalVtxCount;  
    }

    if(drawData->TotalIdxCount > indexCount){
        indexBuffer = VkBuffer(*device, indexBufferSize,
                                vk::BufferUsageFlagBits::eIndexBuffer,
                                vk::MemoryPropertyFlagBits::eHostVisible || vk::MemoryPropertyFlagBits::eHostCoherent);
        indexCount = drawData->TotalIdxCount;
    }

    // BUG: map()/unmap() don't exist on the raw VkBuffer handle (see above). Intended: map both buffers,
    // then append every draw list's vertices/indices back-to-back (vtxDst/idxDst advance per list) so the
    // single vertex/index buffer contains all ImGui geometry of the frame.
    ImDrawVert* vtxDst = static_cast<ImDrawVert*>(vertexBuffer.map());
    ImDrawIdx* idxDst = static_cast<ImDrawIdx*>(indexBuffer.map());

    for(int n = 0; n < drawData->CmdListsCount; n++){
        const ImDrawList* cmdList = drawData->CmdLists[n];
        memcpy(vtxDst, cmdList->VtxBuffer.Data, cmdList->VtxBuffer.Size * sizeof(ImDrawVert));
        memcpy(idxDst, cmdList->IdxBuffer.Data, cmdList->IdxBuffer.Size * sizeof(ImDrawIdx));
        vtxDst += cmdList->VtxBuffer.Size;
        idxDst += cmdList->IdxBuffer.Size;
    }

    vertexBuffer.unmap();
    indexBuffer.unmap();
}

void ImGuiVulkanUtil::drawFrame(vk::raii::CommandBuffer &commandBuffer){
    ImDrawData* drawData = ImGui::GetDrawData();
    if(!drawData || drawData->CmdListsCount == 0){
        return;
    }

    if(drawData->Textures){
        for(int n = 0; n < drawData->Textures->Size; n++){
            ImTextureData* tex = (*drawData->Textures)[n];
            if(tex->Status != ImTextureStatus_OK){
                updateTexture(tex);
            }
        }
    }

    // BUG: colorAttatchment (name misspelled) is left default-initialized - no imageView, no imageLayout,
    // no loadOp/storeOp - so the dynamic-rendering pass targets an empty attachment. Also this local
    // `renderingInfo` shadows the member vk::PipelineRenderingCreateInfo renderingInfo (different type).
    // Intended: begin a dynamic-rendering pass over the swapchain color image view covering the whole
    // framebuffer (drawData->DisplaySize = current logical display size).
    vk::RenderingAttachmentInfo colorAttatchment{};
    vk::RenderingInfo renderingInfo{};
    renderingInfo.renderArea = vk::Rect2D{{0, 0}, {static_cast<uint32_t>(drawData->DisplaySize.x),
                                                    static_cast<uint32_t>(drawData->DisplaySize.y)}};
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttatchment;

    commandBuffer.beginRendering(renderingInfo);

    // BUG: `pipeline` was never created (see initResources), so this binds VK_NULL_HANDLE.
    // Intended: bind the ImGui graphics pipeline before issuing draw commands.
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);

    // BUG: viewport uses the logical DisplaySize without DisplayFramebufferScale - on DPI-scaled
    // displays the framebuffer is larger, so the viewport is wrong.
    // Intended: viewport covering the framebuffer in physical pixels (0..width, 0..height, full depth).
    vk::Viewport viewport{};
    viewport.width = drawData->DisplaySize.x;
    viewport.height = drawData->DisplaySize.y;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    commandBuffer.setViewport(0, viewport);

    // pushConstBlock.scale - converts ImGui pixel coordinates to NDC (2/size).
    // pushConstBlock.translate - NDC offset of the display origin.
    // BUG: translate is hardcoded to (-1,-1); it should be -1 - DisplayPos * scale (per-frame
    // drawData->DisplayPos), so geometry is misplaced whenever the display is not at (0,0)
    // (e.g. multi-viewport or offset displays).
    pushConstBlock.scale = glm::vec2(2.0f / drawData->DisplaySize.x, 2.0f / drawData->DisplaySize.y);
    pushConstBlock.translate = glm::vec2(-1.0f);
    commandBuffer.pushConstants(*pipelineLayout, vk::ShaderStageFlagBits::eVertex, 
                                0, sizeof(PushConstBlock), & pushConstBlock);
    
    int vertexOffset = 0;
    int indexOffset = 0;

    for(int i = 0; i < drawData->CmdListsCount; i++){
        const ImDrawList* cmdList = drawData->CmdLists[i];

        for(int j = 0; j < cmdList->CmdBuffer.Size; j++){
            const ImDrawCmd* pcmd = &cmdList->CmdBuffer[j];

            // BUG: clip rect is not offset by drawData->DisplayPos and not clamped to the framebuffer
            // size, so scissors can be misplaced or oversized on offset/DPI-scaled displays.
            // Intended: pcmd->ClipRect (left, top, right, bottom in pixels) -> Vulkan scissor rectangle.
            vk::Rect2D scissors{};
            scissors.offset.x = std::max(static_cast<int32_t>(pcmd->ClipRect.x), 0);
            scissors.offset.y = std::max(static_cast<int32_t>(pcmd->ClipRect.y), 0);
            scissors.extent.width = static_cast<uint32_t>(pcmd->ClipRect.z - pcmd->ClipRect.x);
            scissors.extent.height = static_cast<uint32_t>(pcmd->ClipRect.w - pcmd->ClipRect.y);
            commandBuffer.setScissor(0, scissors);

            VkDescriptorSet texHandle = (VkDescriptorSet)pcmd->GetTexID();
            if(texHandle){
                commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                                *pipelineLayout, 0, {vk::DescriptorSet(texHandle)}, {});
            } else {
                commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                                *pipelineLayout, 0, {*descriptorSet}, {});
            }

            commandBuffer.drawIndexed(pcmd->ElemCount, 1, indexOffset, vertexOffset, 0);
            indexOffset += pcmd->ElemCount;
        }

        vertexOffset += cmdList->VtxBuffer.Size;
    }

    commandBuffer.endRendering();
}

void ImGuiVulkanUtil::handleKey(int key, int scancode, int action, int mods){
    // key - raw GLFW key code; scancode - platform scancode; action - GLFW_PRESS/RELEASE/REPEAT;
    // mods - GLFW modifier bitmask (Ctrl/Shift/Alt/Super).
    // BUG: GLFW key codes are cast straight to ImGuiKey, but the enums don't match (ImGui named keys
    // start at 512, GLFW letters are ASCII) - most keys map to wrong ImGui keys. scancode and mods are
    // ignored, so no Ctrl/Shift/Alt modifier events are ever emitted.
    // Intended: translate the GLFW key (+ mods) to the equivalent ImGui key event, e.g. via the standard
    // backend key-mapping table, and forward it to io.AddKeyEvent().
    ImGuiIO& io = ImGui::GetIO();

    bool pressed = (action != 0);

    io.AddKeyEvent((ImGuiKey)key, pressed);
}

void ImGuiVulkanUtil::handleMousePos(float x, float y){
    ImGuiIO& io = ImGui::GetIO();

    io.AddMousePosEvent(x, y);
}

void ImGuiVulkanUtil::handleMouseButton(int button, bool pressed){
    ImGuiIO& io = ImGui::GetIO();

    io.AddMouseButtonEvent(button, pressed);
}

bool ImGuiVulkanUtil::getWantKeyCapture(){
    return ImGui::GetIO().WantCaptureKeyboard;
}

void ImGuiVulkanUtil::charPressed(uint32_t key){
    ImGuiIO& io = ImGui::GetIO();
    io.AddInputCharacter(key);
}

};
