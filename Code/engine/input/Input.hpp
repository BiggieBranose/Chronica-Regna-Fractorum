#pragma once
#include <glm/glm.hpp>
#include <unordered_map>
#include <GLFW/glfw3.h>

namespace crf {

enum class Key {
    Unknown = -1,
    A=GLFW_KEY_A, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    Up=GLFW_KEY_UP, Down=GLFW_KEY_DOWN, Left=GLFW_KEY_LEFT, Right=GLFW_KEY_RIGHT,
    Space=GLFW_KEY_SPACE, Enter=GLFW_KEY_ENTER, Escape=GLFW_KEY_ESCAPE,
    LShift=GLFW_KEY_LEFT_SHIFT, RShift=GLFW_KEY_RIGHT_SHIFT,
    LControl=GLFW_KEY_LEFT_CONTROL, RControl=GLFW_KEY_RIGHT_CONTROL,
    LAlt=GLFW_KEY_LEFT_ALT, RAlt=GLFW_KEY_RIGHT_ALT,
    Tab=GLFW_KEY_TAB, Backspace=GLFW_KEY_BACKSPACE, Delete=GLFW_KEY_DELETE,
    F1=GLFW_KEY_F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    Key0=GLFW_KEY_0, Key1, Key2, Key3, Key4, Key5, Key6, Key7, Key8, Key9
};

enum class MouseButton {
    Left = GLFW_MOUSE_BUTTON_LEFT,
    Right = GLFW_MOUSE_BUTTON_RIGHT,
    Middle = GLFW_MOUSE_BUTTON_MIDDLE
};

class Input {
public:
    static Input& instance();

    void init(GLFWwindow* window);
    void beginFrame();
    void endFrame();

    bool isKeyDown(Key key) const;
    bool isKeyPressed(Key key) const;
    bool isKeyReleased(Key key) const;

    bool isMouseDown(MouseButton btn) const;
    bool isMousePressed(MouseButton btn) const;
    bool isMouseReleased(MouseButton btn) const;
    glm::vec2 getMousePos() const;
    glm::vec2 getMouseDelta() const;
    float getScrollDelta() const;

    GLFWwindow* getWindow() const { return m_window; }
    Input() = default;

private:

    GLFWwindow* m_window = nullptr;
    std::unordered_map<int, bool> m_keysPrev;
    std::unordered_map<int, bool> m_mouseDown;
    std::unordered_map<int, bool> m_mousePrev;
    glm::vec2 m_mousePos{};
    glm::vec2 m_mouseDelta{};
    float m_scrollDelta = 0.0f;

    static void scrollCallback(GLFWwindow*, double, double yoffset);
};

} // namespace crf
