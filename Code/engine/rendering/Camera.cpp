#include "Camera.hpp"

namespace crf {

void Camera::setOrthographic(float left, float right, float bottom, float top, float nearZ, float farZ) {
    m_width = right - left;
    m_height = top - bottom;
    (void)nearZ; (void)farZ;
}

void Camera::setSize(float width, float height) {
    m_width = width;
    m_height = height;
}

glm::mat4 Camera::getViewMatrix() const {
    glm::mat4 view = glm::translate(glm::mat4(1.0f), -m_position);
    view = glm::rotate(view, glm::radians(m_rotation), glm::vec3(0.0f, 0.0f, 1.0f));
    return view;
}

glm::mat4 Camera::getProjectionMatrix() const {
    float halfW = m_width * 0.5f / m_zoom;
    float halfH = m_height * 0.5f / m_zoom;
    return glm::ortho(-halfW, halfW, -halfH, halfH, -1.0f, 1.0f);
}

glm::mat4 Camera::getViewProjection() const {
    return getProjectionMatrix() * getViewMatrix();
}

} // namespace crf
