#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace crf {

class Camera {
public:
    Camera() = default;

    void setOrthographic(float left, float right, float bottom, float top, float nearZ = -1.0f, float farZ = 1.0f);
    void setSize(float width, float height);
    void setPosition(glm::vec3 pos) { m_position = pos; }
    void setZoom(float zoom) { m_zoom = zoom; }
    void setRotation(float rot) { m_rotation = rot; }

    glm::vec3 getPosition() const { return m_position; }
    float getZoom() const { return m_zoom; }
    float getRotation() const { return m_rotation; }

    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix() const;
    glm::mat4 getViewProjection() const;

    void move(glm::vec3 delta) { m_position += delta; }
    void zoom(float delta) { m_zoom = glm::max(0.1f, m_zoom + delta); }

private:
    glm::vec3 m_position{0.0f};
    float m_zoom = 1.0f;
    float m_rotation = 0.0f;
    float m_width = 800.0f;
    float m_height = 600.0f;
};

} // namespace crf
