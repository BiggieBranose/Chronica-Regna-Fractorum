#include "Input.hpp"
#include "../core/Log.hpp"

namespace crf {

Input& Input::instance() {
    static Input input;
    return input;
}

void Input::init(GLFWwindow* window) {
    m_window = window;
    glfwSetScrollCallback(window, scrollCallback);
    Log::info("Input system initialized");
}

void Input::beginFrame() {
    double x, y;
    glfwGetCursorPos(m_window, &x, &y);
    glm::vec2 newPos((float)x, (float)y);
    m_mouseDelta = newPos - m_mousePos;
    m_mousePos = newPos;
    m_scrollDelta = 0.0f;
    m_mouseDown[GLFW_MOUSE_BUTTON_LEFT] = glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    m_mouseDown[GLFW_MOUSE_BUTTON_RIGHT] = glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    m_mouseDown[GLFW_MOUSE_BUTTON_MIDDLE] = glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;
}

void Input::endFrame() {
    m_keysPrev.clear();
    m_keysPrev[static_cast<int>(Key::Up)] = isKeyDown(Key::Up);
    m_keysPrev[static_cast<int>(Key::Down)] = isKeyDown(Key::Down);
    m_keysPrev[static_cast<int>(Key::Left)] = isKeyDown(Key::Left);
    m_keysPrev[static_cast<int>(Key::Right)] = isKeyDown(Key::Right);
    m_keysPrev[static_cast<int>(Key::Space)] = isKeyDown(Key::Space);
    m_keysPrev[static_cast<int>(Key::Enter)] = isKeyDown(Key::Enter);
    m_keysPrev[static_cast<int>(Key::Escape)] = isKeyDown(Key::Escape);
    m_keysPrev[static_cast<int>(Key::LShift)] = isKeyDown(Key::LShift);
    m_keysPrev[static_cast<int>(Key::RShift)] = isKeyDown(Key::RShift);
    m_keysPrev[static_cast<int>(Key::LControl)] = isKeyDown(Key::LControl);
    m_keysPrev[static_cast<int>(Key::RControl)] = isKeyDown(Key::RControl);
    for (int k = GLFW_KEY_A; k <= GLFW_KEY_Z; k++)
        m_keysPrev[k] = isKeyDown(static_cast<Key>(k));
    for (int k = GLFW_KEY_0; k <= GLFW_KEY_9; k++)
        m_keysPrev[k] = isKeyDown(static_cast<Key>(k));
    m_mousePrev = m_mouseDown;
}

bool Input::isKeyDown(Key key) const {
    return glfwGetKey(m_window, static_cast<int>(key)) == GLFW_PRESS;
}

bool Input::isKeyPressed(Key key) const {
    int k = static_cast<int>(key);
    auto it = m_keysPrev.find(k);
    return isKeyDown(key) && (it == m_keysPrev.end() || !it->second);
}

bool Input::isKeyReleased(Key key) const {
    int k = static_cast<int>(key);
    auto it = m_keysPrev.find(k);
    return !isKeyDown(key) && it != m_keysPrev.end() && it->second;
}

bool Input::isMouseDown(MouseButton btn) const {
    return glfwGetMouseButton(m_window, static_cast<int>(btn)) == GLFW_PRESS;
}

bool Input::isMousePressed(MouseButton btn) const {
    int b = static_cast<int>(btn);
    auto it = m_mousePrev.find(b);
    return isMouseDown(btn) && (it == m_mousePrev.end() || !it->second);
}

bool Input::isMouseReleased(MouseButton btn) const {
    int b = static_cast<int>(btn);
    auto it = m_mousePrev.find(b);
    return !isMouseDown(btn) && it != m_mousePrev.end() && it->second;
}

glm::vec2 Input::getMousePos() const { return m_mousePos; }
glm::vec2 Input::getMouseDelta() const { return m_mouseDelta; }
float Input::getScrollDelta() const { return m_scrollDelta; }

void Input::scrollCallback(GLFWwindow*, double, double yoffset) {
    instance().m_scrollDelta = (float)yoffset;
}

} // namespace crf
