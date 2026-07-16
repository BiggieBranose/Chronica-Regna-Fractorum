#include "Window.hpp"
#include "core/Log.hpp"
#include "core/Assert.hpp"

#include <cstring>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace crf {

Window::Window(const WindowConfig& cfg) {
    if (!glfwInit()) {
        CRF_ASSERT_MSG(false, "Failed to initialize GLFW");
        return;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, cfg.resizable ? GLFW_TRUE : GLFW_FALSE);

    m_window = glfwCreateWindow(
        static_cast<i32>(cfg.width),
        static_cast<i32>(cfg.height),
        cfg.title.data(),
        nullptr,
        nullptr
    );

    if (!m_window) {
        CRF_ASSERT_MSG(false, "Failed to create GLFW window");
        glfwTerminate();
        return;
    }

    m_width = cfg.width;
    m_height = cfg.height;

    glfwSetWindowUserPointer(m_window, this);
    glfwSetKeyCallback(m_window, glfwKeyCallback);
    glfwSetMouseButtonCallback(m_window, glfwMouseButtonCallback);
    glfwSetCursorPosCallback(m_window, glfwCursorPosCallback);
    glfwSetFramebufferSizeCallback(m_window, glfwFramebufferSizeCallback);

    crf::Log::info("Window created: {}x{}", m_width, m_height);
}

Window::~Window() {
    if (m_window) {
        glfwDestroyWindow(m_window);
        crf::Log::info("Window destroyed");
    }
    glfwTerminate();
}

bool Window::shouldClose() const {
    return glfwWindowShouldClose(m_window);
}

void Window::pollEvents() const {
    std::memcpy(const_cast<bool*>(m_keysPrev), m_keys, s_keyCount);
    std::memcpy(const_cast<bool*>(m_mouseButtonsPrev), m_mouseButtons, s_mouseButtonCount);
    glfwPollEvents();
}

void Window::waitEvents() const {
    std::memcpy(const_cast<bool*>(m_keysPrev), m_keys, s_keyCount);
    std::memcpy(const_cast<bool*>(m_mouseButtonsPrev), m_mouseButtons, s_mouseButtonCount);
    glfwWaitEvents();
}

VkSurfaceKHR Window::createSurface(VkInstance instance) const {
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkResult result = glfwCreateWindowSurface(instance, m_window, nullptr, &surface);
    if (result != VK_SUCCESS) {
        crf::Log::error("Failed to create Vulkan surface, error: {}", static_cast<i32>(result));
        return VK_NULL_HANDLE;
    }
    return surface;
}

bool Window::isKeyPressed(i32 key) const {
    if (key < 0 || key >= s_keyCount) return false;
    return m_keys[key];
}

bool Window::isKeyJustPressed(i32 key) const {
    if (key < 0 || key >= s_keyCount) return false;
    return m_keys[key] && !m_keysPrev[key];
}

bool Window::isMouseButtonPressed(i32 button) const {
    if (button < 0 || button >= s_mouseButtonCount) return false;
    return m_mouseButtons[button];
}

bool Window::isMouseButtonJustPressed(i32 button) const {
    if (button < 0 || button >= s_mouseButtonCount) return false;
    return m_mouseButtons[button] && !m_mouseButtonsPrev[button];
}

f32 Window::getMouseX() const { return m_mouseX; }
f32 Window::getMouseY() const { return m_mouseY; }
f32 Window::getMouseDeltaX() const { return m_mouseDeltaX; }
f32 Window::getMouseDeltaY() const { return m_mouseDeltaY; }

void Window::glfwKeyCallback(GLFWwindow* window, i32 key, i32 /*scancode*/, i32 action, i32 /*mods*/) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (key < 0 || key >= s_keyCount) return;

    if (action == GLFW_PRESS) {
        self->m_keys[key] = true;
    } else if (action == GLFW_RELEASE) {
        self->m_keys[key] = false;
    }
}

void Window::glfwMouseButtonCallback(GLFWwindow* window, i32 button, i32 action, i32 /*mods*/) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (button < 0 || button >= s_mouseButtonCount) return;

    if (action == GLFW_PRESS) {
        self->m_mouseButtons[button] = true;
    } else if (action == GLFW_RELEASE) {
        self->m_mouseButtons[button] = false;
    }
}

void Window::glfwCursorPosCallback(GLFWwindow* window, f64 xpos, f64 ypos) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    auto fx = static_cast<f32>(xpos);
    auto fy = static_cast<f32>(ypos);

    if (self->m_firstMouse) {
        self->m_prevMouseX = fx;
        self->m_prevMouseY = fy;
        self->m_firstMouse = false;
    }

    self->m_mouseDeltaX = fx - self->m_prevMouseX;
    self->m_mouseDeltaY = fy - self->m_prevMouseY;
    self->m_prevMouseX = fx;
    self->m_prevMouseY = fy;
    self->m_mouseX = fx;
    self->m_mouseY = fy;
}

void Window::glfwFramebufferSizeCallback(GLFWwindow* window, i32 width, i32 height) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    self->m_width = static_cast<u32>(width);
    self->m_height = static_cast<u32>(height);
    self->m_resized = true;
}

} // namespace crf
