#include "Model.hpp"
#include "../core/Log.hpp"
#include "../../external/VMA/vk_mem_alloc.h"

namespace crf {

Model::~Model() {}

bool Model::loadOBJ(VkDevice device, VmaAllocator allocator, const std::string& filepath) {
    (void)device; (void)allocator;
    Log::info("Model loading not yet implemented: {}", filepath);
    return false;
}

void Model::destroy(VkDevice device, VmaAllocator allocator) {
    (void)device;
    for (auto& mesh : m_meshes) {
        if (mesh.vertexBuffer)
            vmaDestroyBuffer(static_cast<VmaAllocator>(allocator), mesh.vertexBuffer, mesh.vertexAllocation);
        if (mesh.indexBuffer)
            vmaDestroyBuffer(static_cast<VmaAllocator>(allocator), mesh.indexBuffer, mesh.indexAllocation);
    }
    m_meshes.clear();
}

void Model::draw(vk::raii::CommandBuffer& cmd) const {
    (void)cmd;
}

} // namespace crf
