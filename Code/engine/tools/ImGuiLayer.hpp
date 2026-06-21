#pragma once
#include <vulkan/vulkan_raii.hpp>
#include <vector>
#include <memory>

typedef struct VmaAllocator_T* VmaAllocator;

namespace crf {

class Engine;

class ImGuiLayer {
public:
    ImGuiLayer() = default;
    ~ImGuiLayer();

    bool initialize(Engine* engine);
    void shutdown();

    void beginFrame();
    void endFrame(vk::raii::CommandBuffer& cmd);

private:
    Engine* m_engine = nullptr;
    bool m_initialized = false;
};

} // namespace crf
