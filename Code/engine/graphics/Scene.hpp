#pragma once

#include "core/Types.hpp"
#include "GlTFLoader.hpp"
#include "Transform.hpp"
#include <string>
#include <vector>

namespace crf {

struct Entity {
    std::string name;
    u32 meshIndex = 0;
    glm::mat4 transform{1.0f};
    u32 firstPrimitive = 0;
    u32 primitiveCount = 0;
    bool visible = true;
};

struct Trigger {
    std::string name;
    AABB bounds;
};

class Scene {
public:
    // Loads a .glb; named nodes are parsed into entities, colliders and triggers.
    // - nodes named "collider_*" become static AABB colliders
    // - nodes named "trigger_*" become trigger volumes
    // - everything else becomes a visible entity
    void loadSceneFile(const std::string& filepath);

    // Loads a mesh that can be spawned as a dynamic entity (e.g. the player character).
    // Returns the mesh index to pass to addEntity.
    u32 loadMeshFile(const std::string& filepath);

    u32 addEntity(const std::string& name, u32 meshIndex, const glm::mat4& transform);

    Entity* findEntity(const std::string& name);
    const Entity* findEntity(const std::string& name) const;
    Entity& getEntity(u32 index) { return m_entities[index]; }
    const Entity& getEntity(u32 index) const { return m_entities[index]; }

    // XZ-plane collision query for movement (2.5D).
    bool isBlocked(glm::vec2 pos, f32 radius) const;

    const std::vector<MeshData>& getMeshes() const { return m_meshes; }
    const std::vector<Entity>& getEntities() const { return m_entities; }
    const std::vector<AABB>& getColliders() const { return m_colliders; }
    const std::vector<Trigger>& getTriggers() const { return m_triggers; }

    // Flattened global texture list across all loaded meshes.
    const std::vector<ImageData>& getImages() const { return m_images; }
    // Descriptor-set offset per mesh: primitive.textureIndex + getTextureOffset(mesh) indexes m_images.
    u32 getTextureOffset(u32 meshIndex) const { return m_textureOffsets[meshIndex]; }

private:
    void appendMesh(const MeshData& mesh);

    std::vector<MeshData> m_meshes;
    std::vector<Entity> m_entities;
    std::vector<AABB> m_colliders;
    std::vector<Trigger> m_triggers;
    std::vector<ImageData> m_images;
    std::vector<u32> m_textureOffsets;
};

}
