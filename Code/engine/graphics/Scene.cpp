#include "Scene.hpp"
#include <core/Log.hpp>

#include <cstring>

namespace crf {

void Scene::appendMesh(const MeshData& mesh) {
    m_textureOffsets.push_back(static_cast<u32>(m_images.size()));
    for (const ImageData& image : mesh.images) {
        m_images.push_back(image);
    }
    m_meshes.push_back(mesh);
}

void Scene::loadSceneFile(const std::string& filepath) {
    const MeshData& mesh = loadScene(filepath);
    const u32 meshIndex = static_cast<u32>(m_meshes.size());
    appendMesh(mesh);

    for (const NodeData& node : mesh.nodes) {
        AABB bounds{node.aabbMin, node.aabbMax};

        if (node.name.rfind("col_", 0) == 0) {
            m_physics.addCollider(bounds);
            Log::info("Scene: collider '{}' at ({:.1f},{:.1f})-({:.1f},{:.1f})",
                node.name, bounds.min.x, bounds.min.z, bounds.max.x, bounds.max.z);
            continue;
        }

        if (node.name.rfind("trg_", 0) == 0) {
            m_physics.addTrigger(node.name, bounds);
            Log::info("Scene: trigger '{}' at ({:.1f},{:.1f})-({:.1f},{:.1f})",
                node.name, bounds.min.x, bounds.min.z, bounds.max.x, bounds.max.z);
            continue;
        }

        Entity entity;
        entity.name = node.name;
        entity.meshIndex = meshIndex;
        entity.transform = node.worldTransform;
        entity.bounds = bounds;
        entity.firstPrimitive = node.firstPrimitive;
        entity.primitiveCount = node.primitiveCount;
        m_entities.push_back(entity);
        Log::info("Scene: entity '{}' mesh={} prims=[{},{})",
            node.name, entity.meshIndex, entity.firstPrimitive,
            entity.firstPrimitive + entity.primitiveCount);
    }
}

u32 Scene::loadMeshFile(const std::string& filepath) {
    const MeshData& mesh = loadScene(filepath);
    const u32 meshIndex = static_cast<u32>(m_meshes.size());
    appendMesh(mesh);
    Log::info("Scene: loaded mesh '{}' as mesh index {}", filepath, meshIndex);
    return meshIndex;
}

u32 Scene::addEntity(const std::string& name, u32 meshIndex, const glm::mat4& transform) {
    Entity entity;
    entity.name = name;
    entity.meshIndex = meshIndex;
    entity.transform = transform;
    entity.firstPrimitive = 0;
    entity.primitiveCount = static_cast<u32>(m_meshes[meshIndex].primitives.size());
    const MeshData& mesh = m_meshes[meshIndex];
    if (!mesh.nodes.empty()) {
        // A mesh file loaded via loadMeshFile has a single root node holding its local bounds.
        const AABB local{mesh.nodes[0].aabbMin, mesh.nodes[0].aabbMax};
        entity.bounds = local.transformedBy(transform);
    }
    m_entities.push_back(entity);
    Log::info("Scene: added entity '{}' mesh={} at ({:.1f},{:.1f},{:.1f})",
        name, meshIndex, transform[3][0], transform[3][1], transform[3][2]);
    return static_cast<u32>(m_entities.size() - 1);
}

Entity* Scene::findEntity(const std::string& name) {
    for (Entity& entity : m_entities) {
        if (entity.name == name) {
            return &entity;
        }
    }
    return nullptr;
}

const Entity* Scene::findEntity(const std::string& name) const {
    for (const Entity& entity : m_entities) {
        if (entity.name == name) {
            return &entity;
        }
    }
    return nullptr;
}

}
