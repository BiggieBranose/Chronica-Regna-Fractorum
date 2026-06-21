#include "Transform.hpp"

namespace crf {

glm::mat4 Transform::getLocalMatrix() {
    if (m_dirty) {
        m_localMatrix = glm::translate(glm::mat4(1.0f), m_position);
        m_localMatrix = glm::rotate(m_localMatrix, glm::radians(m_rotation), glm::vec3(0.0f, 0.0f, 1.0f));
        m_localMatrix = glm::scale(m_localMatrix, m_scale);
        m_dirty = false;
    }
    return m_localMatrix;
}

glm::mat4 Transform::getWorldMatrix() {
    glm::mat4 local = getLocalMatrix();
    if (m_parent)
        m_worldMatrix = m_parent->getWorldMatrix() * local;
    else
        m_worldMatrix = local;
    return m_worldMatrix;
}

} // namespace crf
