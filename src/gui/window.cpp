#include "window.hpp"

Window::Window(int width, int height, const std::string& name) : m_width(width), m_height(height), m_name(name) {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);  // Do not create OpenGL context

    m_window = glfwCreateWindow(m_width, m_height, m_name.c_str(), nullptr, nullptr);
    glfwSetWindowUserPointer(m_window, this);  // Give glfw pointer to this class so we can access it in callback functions.
    glfwSetFramebufferSizeCallback(m_window, framebufferResizeCallback);
}

GLFWwindow* Window::handle() const {
    return m_window;
}

void Window::framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    Window* win = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
    win->m_framebufferResized = true;
    win->m_width = width;
    win->m_height = height;
}

bool Window::shouldClose() const {
    return glfwWindowShouldClose(m_window);
}

void Window::update() const {
    glfwPollEvents();
}

void Window::stopWhileMinimized() const {
    int width = 0, height = 0;
    glfwGetFramebufferSize(m_window, &width, &height);

    while(width == 0 || height == 0) {
        glfwGetFramebufferSize(m_window, &width, &height);
        glfwWaitEvents();
    }
}

bool Window::resized() {
    const bool val = m_framebufferResized;
    m_framebufferResized = false;

    return val;
}

Window::~Window() {
    glfwDestroyWindow(m_window);
    glfwTerminate();
}
