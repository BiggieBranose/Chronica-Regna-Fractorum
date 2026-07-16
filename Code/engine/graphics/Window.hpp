#pragma once

#include "core/Types.hpp"
#include "core/Platform.hpp"
#include <string_view>

struct VkInstance_T;
using VkInstance = VkInstance_T*;
struct VkSurfaceKHR_T;
using VkSurfaceKHR = VkSurfaceKHR_T*;

struct GLFWwindow;

namespace crf {

struct WindowConfig {
    std::string_view title = "Chronica Regna Fractorum";
    u32 width = 1280;
    u32 height = 720;
    bool resizable = true;
    bool vsync = true;
};

class Window {
public:
    Window(const WindowConfig& cfg = {});
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;

    bool shouldClose() const;
    void pollEvents() const;
    void waitEvents() const;

    VkSurfaceKHR createSurface(VkInstance instance) const;

    bool isKeyPressed(i32 key) const;
    bool isKeyJustPressed(i32 key) const;
    bool isMouseButtonPressed(i32 button) const;
    bool isMouseButtonJustPressed(i32 button) const;

    f32 getMouseX() const;
    f32 getMouseY() const;
    f32 getMouseDeltaX() const;
    f32 getMouseDeltaY() const;

    u32 getWidth() const { return m_width; }
    u32 getHeight() const { return m_height; }
    f32 getAspect() const { return static_cast<f32>(m_width) / static_cast<f32>(m_height); }
    bool wasResized() const { return m_resized; }
    void clearResized() { m_resized = false; }

    GLFWwindow* getHandle() const { return m_window; }

private:
    static void glfwKeyCallback(GLFWwindow* window, i32 key, i32 scancode, i32 action, i32 mods);
    static void glfwMouseButtonCallback(GLFWwindow* window, i32 button, i32 action, i32 mods);
    static void glfwCursorPosCallback(GLFWwindow* window, f64 xpos, f64 ypos);
    static void glfwFramebufferSizeCallback(GLFWwindow* window, i32 width, i32 height);

    GLFWwindow* m_window = nullptr;
    u32 m_width = 0;
    u32 m_height = 0;
    bool m_resized = false;

    static constexpr i32 s_keyCount = 350;
    static constexpr i32 s_mouseButtonCount = 8;
    bool m_keys[s_keyCount] = {};
    bool m_keysPrev[s_keyCount] = {};
    bool m_mouseButtons[s_mouseButtonCount] = {};
    bool m_mouseButtonsPrev[s_mouseButtonCount] = {};
    f32 m_mouseX = 0.0f;
    f32 m_mouseY = 0.0f;
    f32 m_mouseDeltaX = 0.0f;
    f32 m_mouseDeltaY = 0.0f;
    f32 m_prevMouseX = 0.0f;
    f32 m_prevMouseY = 0.0f;
    bool m_firstMouse = true;
};

} // namespace crf
