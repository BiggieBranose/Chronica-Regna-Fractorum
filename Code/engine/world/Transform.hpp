#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>

namespace crf {

class Transform {
public:
    Transform() = default;

    void setPosition(glm::vec3 pos) { m_position = pos; m_dirty = true; }
    void setRotation(float rot) { m_rotation = rot; m_dirty = true; }
    void setScale(glm::vec3 scale) { m_scale = scale; m_dirty = true; }
    void setScale(float s) { m_scale = glm::vec3(s); m_dirty = true; }

    glm::vec3 getPosition() const { return m_position; }
    float getRotation() const { return m_rotation; }
    glm::vec3 getScale() const { return m_scale; }

    void translate(glm::vec3 delta) { m_position += delta; m_dirty = true; }
    void rotate(float delta) { m_rotation += delta; m_dirty = true; }
    void scaleBy(glm::vec3 delta) { m_scale *= delta; m_dirty = true; }

    glm::mat4 getLocalMatrix();
    glm::mat4 getWorldMatrix();

    void setParent(Transform* parent) { m_parent = parent; m_dirty = true; }
    Transform* getParent() const { return m_parent; }

private:
    glm::vec3 m_position{0.0f};
    float m_rotation = 0.0f;
    glm::vec3 m_scale{1.0f};
    bool m_dirty = true;
    glm::mat4 m_localMatrix{1.0f};
    glm::mat4 m_worldMatrix{1.0f};
    Transform* m_parent = nullptr;
};

} // namespace crf
